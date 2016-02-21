

#include <WinSock2.h>
#include <stdio.h>
#include <initguid.h>
#include <ws2bth.h>
#include <strsafe.h>
#include <stdint.h>

#include "ring_buffer.h"

// {B62C4E8D-62CC-404b-BBBF-BF3E3BBB1374}
DEFINE_GUID(g_guidServiceClass, 0xb62c4e8d, 0x62cc, 0x404b, 0xbb, 0xbf, 0xbf, 0x3e, 0x3b, 0xbb, 0x13, 0x74);

#define CXN_TEST_DATA_STRING              (L"~!@#$%^&*()-_=+?<>1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")
#define CXN_TRANSFER_DATA_LENGTH          (sizeof(CXN_TEST_DATA_STRING))

#define DEFAULT_LISTEN_BACKLOG        4
#define INSTANCE_STR L"BluetoothWindows"

#define LEN_RECV 256

/*
Just an example of data structure.
*/
#define LEN_HEADER 10 + 6 + 6
typedef struct packet_s
{
    char header[LEN_HEADER];
    //int32_t x;
    //int32_t y;
} packet_t;


static void print_error(char const *where, int code)
{
    fprintf(stderr, "Error on %s: code %d\n", where, code);
}

static BOOL bind_socket(SOCKET local_socket, SOCKADDR_BTH *sock_addr_bth_local)
{
    int addr_len = sizeof(SOCKADDR_BTH);

    /* Setting address family to AF_BTH indicates winsock2 to use Bluetooth port. */
    sock_addr_bth_local->addressFamily = AF_BTH;
    sock_addr_bth_local->port = BT_PORT_ANY;

    if (bind(local_socket, (struct sockaddr *) sock_addr_bth_local, sizeof(SOCKADDR_BTH)) == SOCKET_ERROR) {
        print_error("bind()", WSAGetLastError());
        return FALSE;
    }

    if (getsockname(local_socket, (struct sockaddr *)sock_addr_bth_local, &addr_len) == SOCKET_ERROR) {
        print_error("getsockname()", WSAGetLastError());
        return FALSE;
    }
    return TRUE;
}

static LPCSADDR_INFO create_addr_info(SOCKADDR_BTH *sock_addr_bth_local)
{
    LPCSADDR_INFO addr_info = calloc(1, sizeof(CSADDR_INFO));

    if (addr_info == NULL) {
        print_error("malloc(addr_info)", WSAGetLastError());
        return NULL;
    }

    addr_info[0].LocalAddr.iSockaddrLength = sizeof(SOCKADDR_BTH);
    addr_info[0].LocalAddr.lpSockaddr = (LPSOCKADDR)sock_addr_bth_local;
    addr_info[0].RemoteAddr.iSockaddrLength = sizeof(SOCKADDR_BTH);
    addr_info[0].RemoteAddr.lpSockaddr = (LPSOCKADDR)&sock_addr_bth_local;
    addr_info[0].iSocketType = SOCK_STREAM;
    addr_info[0].iProtocol = BTHPROTO_RFCOMM;
    return addr_info;
}

/*
instance_name is a pointer to wchar_t* which is malloc'ed by this function.
Must be free manually after.
*/
static BOOL advertise_service_accepted(LPCSADDR_INFO addr_info, wchar_t **instance_name)
{
    WSAQUERYSET wsa_query_set = { 0 };
    wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD len_computer_name = MAX_COMPUTERNAME_LENGTH + 1;
    size_t instance_name_size = 0;
    HRESULT res;

    if (!GetComputerName(computer_name, &len_computer_name)) {
        print_error("GetComputerName()", WSAGetLastError());
        return FALSE;
    }

    /*
    Adding a byte to the size to account for the space in the
    format string in the swprintf call. This will have to change if converted
    to UNICODE.
    */
    res = StringCchLength(computer_name, sizeof(computer_name), &instance_name_size);
    if (FAILED(res)) {
        print_error("ComputerName specified is too large", WSAGetLastError());
        return FALSE;
    }

    instance_name_size += sizeof(INSTANCE_STR) + 1;
    *instance_name = malloc(instance_name_size);
    if (*instance_name == NULL) {
        print_error("malloc(instance_name)", WSAGetLastError());
        return FALSE;
    }

    /* If we got an address, go ahead and advertise it. */
    ZeroMemory(&wsa_query_set, sizeof(wsa_query_set));
    wsa_query_set.dwSize = sizeof(wsa_query_set);
    wsa_query_set.lpServiceClassId = (LPGUID)&g_guidServiceClass;

    StringCbPrintf(*instance_name, instance_name_size, L"%s %s", computer_name, INSTANCE_STR);
    wsa_query_set.lpszServiceInstanceName = *instance_name;
    wsa_query_set.lpszComment = L"Example of server on Windows expecting bluetooth connections";
    wsa_query_set.dwNameSpace = NS_BTH;
    wsa_query_set.dwNumberOfCsAddrs = 1; /* Must be 1. */
    wsa_query_set.lpcsaBuffer = addr_info; /* Req'd */

    /*
    As long as we use a blocking accept(), we will have a race between advertising the service and actually being ready to
    accept connections.  If we use non-blocking accept, advertise the service after accept has been called.
    */
    if (WSASetService(&wsa_query_set, RNRSERVICE_REGISTER, 0) == SOCKET_ERROR) {
        free(instance_name);
        print_error("WSASetService()", WSAGetLastError());
        return FALSE;
    }
    return TRUE;
}

BOOL receive_data(SOCKET client_socket, ring_buffer_t *rb)
{
    char *buffer = NULL;
    int len_read = 0;

    buffer = calloc(LEN_RECV, sizeof(char*));
    if (buffer == NULL) {
        print_error("malloc(buffer)", WSAGetLastError());
        return FALSE;
    }

    len_read = recv(client_socket, buffer, LEN_RECV, 0);
    if (len_read == SOCKET_ERROR) {
        free(buffer);
        print_error("recv()", WSAGetLastError());
        return FALSE;
    }
    if (len_read == 0) {
        free(buffer);
        fprintf(stderr, "Nothing read, end of communication\n");
        return FALSE;
    }
    push_data_in_ring_buffer(rb, buffer, len_read);
    free(buffer);
    return TRUE;
}

void handle_data_read(ring_buffer_t *rb)
{
    packet_t *packet;

    packet = (packet_t*) pop_data_from_ring_buffer(rb, sizeof(packet_t));
    if (packet == NULL) {
        fprintf(stderr, "Cannot handle packet read\n");
        return;
    }
    /* printf("Packet: header=[%s] x=%d, y=%d\n", packet->header, packet->x, packet->y); */
    for (int i = 0; i < LEN_HEADER; ++i) {
        printf("%c", packet->header[i]);
    }
    printf("\n");
    free(packet);
}

ULONG run_server_mode()
{
    wchar_t *       instance_name = NULL;
    SOCKET          local_socket = INVALID_SOCKET;
    SOCKADDR_BTH    sock_addr_bth_local = { 0 };
    LPCSADDR_INFO   addr_info = NULL;
    ring_buffer_t   *rb = NULL;
    BOOL ret = FALSE;

    /* Open a bluetooth socket using RFCOMM protocol. */
    local_socket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (local_socket == INVALID_SOCKET) {
        print_error("socket()", WSAGetLastError());
        return FALSE;
    }

    ret = bind_socket(local_socket, &sock_addr_bth_local);
    if (!ret) {
        return FALSE;
    }
    addr_info = create_addr_info(&sock_addr_bth_local);
    if (!addr_info) {
        return FALSE;
    }
    ret = advertise_service_accepted(addr_info, &instance_name);
    if (!ret) {
        free(addr_info);
        if (instance_name) {
            free(instance_name);
        }
        return FALSE;
    }

    if (listen(local_socket, DEFAULT_LISTEN_BACKLOG) == SOCKET_ERROR) {
        print_error("listen()", WSAGetLastError());
        free(addr_info);
        free(instance_name);
        return FALSE;
    }

    while (1) {
        printf("Waiting for client connection...");
        SOCKET client_socket = accept(local_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET) {
            print_error("accept()", WSAGetLastError());
            return FALSE;
        }
        printf("Client connected !\n");
        rb = create_ring_buffer(LEN_RECV * 3);

        ret = TRUE;
        while (ret == TRUE) {
            ret = receive_data(client_socket, rb);
            while (rb->count >= sizeof(packet_t)) {
                handle_data_read(rb);
            }
        }
        printf("Communication over\n");
        closesocket(client_socket);
        delete_ring_buffer(rb);
    }

    free(addr_info);
    free(instance_name);
    closesocket(local_socket);
    return TRUE;
}

int main()
{
    WSADATA WSAData = { 0 };
    int ret = 0;

    printf("Start the server...\n");
    ret = WSAStartup(MAKEWORD(2, 2), &WSAData);
    if (ret < 0) {
        print_error("WSAStartup()", GetLastError());
        return EXIT_FAILURE;
    }
 
    run_server_mode();

    system("pause");
    return EXIT_SUCCESS;
}
