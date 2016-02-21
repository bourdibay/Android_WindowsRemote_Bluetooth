package com.bourdi_bay.bluetoothandroid;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.widget.Toast;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Set;
import java.util.UUID;

public class MainActivity extends AppCompatActivity {

    private static final int REQUEST_ENABLE_BT = 2;
    // Must be the same as the one on the server.
    private static final UUID MY_UUID = UUID.fromString("B62C4E8D-62CC-404b-BBBF-BF3E3BBB1374");
    public BluetoothAdapter mBluetoothAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (mBluetoothAdapter == null) {
            // Device does not support Bluetooth
            Toast.makeText(this, "No bluetooth supported", Toast.LENGTH_LONG).show();
            return;
        }
        if (!mBluetoothAdapter.isEnabled()) {
            Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
        }

        Set<BluetoothDevice> pairedDevices = mBluetoothAdapter.getBondedDevices();
        if (pairedDevices.size() > 0) {
            // Try to connect to the first paired device.
            // In real use case we should ask the user to select which one he wants to connect to.
            BluetoothDevice deviceToConnect = pairedDevices.iterator().next();
            new Thread(new ConnectThread(deviceToConnect)).start();
        } else {
            Toast.makeText(this, "No device paired...", Toast.LENGTH_LONG).show();
            return;
        }
    }

    private class ConnectThread extends Thread {
        private BluetoothSocket mmSocket = null;
        private OutputStream mOutput;

        public ConnectThread(BluetoothDevice device) {
            // Get a BluetoothSocket to connect with the given BluetoothDevice
            try {
                mmSocket = device.createRfcommSocketToServiceRecord(MY_UUID);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        public void run() {
            // Cancel discovery because it will slow down the connection
            mBluetoothAdapter.cancelDiscovery();

            try {
                // Connect the device through the socket. This will block
                // until it succeeds or throws an exception
                mmSocket.connect();
            } catch (IOException connectException) {
                // Unable to connect; close the socket and get out
                try {
                    mmSocket.close();
                } catch (IOException closeException) {
                    closeException.printStackTrace();
                }
                connectException.printStackTrace();
                return;
            }

            try {
                mOutput = mmSocket.getOutputStream();
            } catch (IOException e) {
                e.printStackTrace();
                return;
            }

            // Packet sent over the bluetooth connection.
            // The server expects a string of 10+6+6 chars for this test.
            class PacketTest {
                char header[] = new char[10];
                char x[] = new char[6];
                char y[] = new char[6];

                public PacketTest(String s, int x, int y) {
                    for (int i = 0; i < 10 && i < s.length(); ++i) {
                        header[i] = s.charAt(i);
                    }
                    final String sx = String.valueOf(x);
                    for (int i = 0; i < 6 && i < sx.length(); ++i) {
                        this.x[i] = sx.charAt(i);
                    }
                    final String sy = String.valueOf(y);
                    for (int i = 0; i < 6 && i < sy.length(); ++i) {
                        this.y[i] = sy.charAt(i);
                    }
                }

                public String toString() {
                    StringBuilder sb = new StringBuilder(10 + 6 + 6);
                    sb.append(header);
                    sb.append(x);
                    sb.append(y);
                    return sb.toString();
                }
            }

            for (int x = 0; x < 10; ++x) {
                for (int y = 0; y < 10; ++y) {
                    PacketTest p = new PacketTest("Hello", x, y);

                    final String str = p.toString();
                    Log.d("TEST", String.format("Gonna write [%s]", str));
                    // Send to the server !
                    write(str.getBytes());
                }
            }
            cancel();
        }

        public void write(byte[] bytes) {
            try {
                mOutput.write(bytes);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        /**
         * Will cancel an in-progress connection, and close the socket
         */
        public void cancel() {
            try {
                mmSocket.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }
}
