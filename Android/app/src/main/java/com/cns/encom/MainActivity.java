package com.cns.encom;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.hardware.camera2.CameraManager;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.preference.PreferenceManager;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.SurfaceHolder;

import android.view.WindowManager;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import net.majorkernelpanic.streaming.gl.SurfaceView;
import net.majorkernelpanic.streaming.SessionBuilder;
import net.majorkernelpanic.streaming.rtsp.RtspServer;
import net.majorkernelpanic.streaming.Session;
import net.majorkernelpanic.streaming.SessionBuilder;
import net.majorkernelpanic.streaming.audio.AudioQuality;
import net.majorkernelpanic.streaming.gl.SurfaceView;
import net.majorkernelpanic.streaming.video.VideoQuality;

import java.net.InetAddress;
import java.io.*;
import java.net.*;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity implements SurfaceHolder.Callback{

    boolean switchCam = false; //switch3
//    boolean switchMic = false; //switch2
    WifiManager wifiMgr;
    CameraManager camMgr;
    private SurfaceView mSurfaceView;
    private SurfaceHolder mSurfaceHolder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        Context context = getApplicationContext();
        wifiMgr = (WifiManager) context.getSystemService(context.WIFI_SERVICE);
        camMgr = (CameraManager) context.getSystemService(context.CAMERA_SERVICE);

        TextView term = findViewById(R.id.textView);
        term.setMovementMethod(new ScrollingMovementMethod());

        mSurfaceView = findViewById(R.id.surfaceView);
        mSurfaceHolder = mSurfaceView.getHolder();
        mSurfaceHolder.addCallback(this);

        SessionBuilder.getInstance()
                .setSurfaceView(mSurfaceView)
                .setPreviewOrientation(0)
                .setContext(getApplicationContext())
                .setAudioEncoder(SessionBuilder.AUDIO_NONE)
                .setVideoEncoder(SessionBuilder.VIDEO_H264);

        // Sets the port of the RTSP server to 1234
        SharedPreferences.Editor editor = PreferenceManager.getDefaultSharedPreferences(this).edit();
        editor.putString(RtspServer.KEY_PORT, String.valueOf(1234));
        editor.apply();

        this.startService(new Intent(this,RtspServer.class));
    }

    @Override
    protected void onResume() {
        super.onResume();
        // ***************************Display device IP***************************
        WifiInfo wifiInfo = wifiMgr.getConnectionInfo();
        int ip = wifiInfo.getIpAddress();
        ByteBuffer buffer = ByteBuffer.allocate(4);
        buffer.putInt(ip);
        InetAddress inetAddressRestored;
        String strip=null;
        try{
            inetAddressRestored = InetAddress.getByAddress(buffer.array());
            strip = inetAddressRestored.getHostAddress();
        }catch (UnknownHostException e){
            strip = "error";
        }
        String[] ips={"0","0","0","0"};
        try {
            //Only works on ipv4!
            ips = strip.split("\\.");
        }catch(Exception e){
            Log.d("Start up", "onCreate: "+e.getMessage());
        }
        String tip = ips[3]+"."+ips[2]+"."+ips[1]+"."+ips[0];
        TextView myip = findViewById(R.id.Indicator_myip);
        myip.setText("Device IP: "+tip);

        // ***************************Start of Display available cameras***************************

        TextView logsWindows = findViewById(R.id.textView);

        try {
            logsWindows.append("Online cameras: "+Arrays.toString(camMgr.getCameraIdList())+"\n");
        }catch (Exception e){
            logsWindows.append(e.getMessage());
        }

        if (ActivityCompat.checkSelfPermission(this,
                Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.CAMERA}, 1);
        }
    }

    public void clear_oncliek(View view){
        Toast.makeText(view.getContext(), "Console cleared", Toast.LENGTH_SHORT).show();
        TextView logsWindows = findViewById(R.id.textView);
        logsWindows.setText("");
    }

    public void scanLocal(View view){
        WifiInfo wifiInfo = wifiMgr.getConnectionInfo();
        int ip = wifiInfo.getIpAddress();
        ByteBuffer buffer = ByteBuffer.allocate(4);
        buffer.putInt(ip);
        InetAddress inetAddressRestored;
        String strip=null;
        try{
            inetAddressRestored = InetAddress.getByAddress(buffer.array());
            strip = inetAddressRestored.getHostAddress();
        }catch (UnknownHostException e){
            strip = "error";
        }
        TextView logsWindows = findViewById(R.id.textView);
        Log.d("ScanNet", strip);

        String[] ips={"0","0","0","0"};
        try {
            //Only works on ipv4!
            ips = strip.split("\\.");
        }catch(Exception e){
            Toast.makeText(view.getContext(), e.getMessage(), Toast.LENGTH_SHORT).show();
        }
            int sub = Integer.parseInt(ips[3]);
            Log.d("Sub", String.valueOf(sub));
            logsWindows.append("\nTry scanning on subnet\n");

            new Thread(new scannerRunner(ips, logsWindows)).start();
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {

    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int i, int i1, int i2) {

    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {

    }

    public class scannerRunner implements Runnable
    {
        String[] ips;
        TextView view;
        public scannerRunner(String[] ips, TextView view)
        {
            this.ips = ips;
            this.view = view;
        }

        public scannerRunner(String tarip, int i, View view) {

        }

        @Override
        public void run() {

            ExecutorService pool = Executors.newFixedThreadPool(10);
            for (int i = 0; i <= 255; i+=10){


                for(int j = 0; j < 10; j++)
                {
                    final String target = ips[3]+"."+ips[2]+"."+ips[1]+"."+String.valueOf(i+j);
                    final int ij = i + j;
                    pool.submit(new Runnable() {
                        @Override
                        public void run() {
                            boolean alive=false;
                            try {
                                final InetAddress tarmachine = InetAddress.getByName(target);
                                alive=tarmachine.isReachable(400);

                            }catch(Exception e) {
                                Log.d("ScanNet", "run: " + ij + " " + e.getMessage());
                                alive=false;
                            }
                            final String targetCopy = target;
                            final boolean aliveCopy = alive;
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    if (aliveCopy) view.append(targetCopy + " Alive \n");
                                }
                            });
                        }
                    });
                }


            }
        }
    }

    public void connect_onclick(View view){
        EditText editText=findViewById(R.id.ip_form);
        String tarip=editText.getText().toString();
        TextView logsWindows = findViewById(R.id.textView);
        logsWindows.append("Target machine IP: "+tarip+"\n");
        TextView indicator_host = findViewById(R.id.Indicator_hostip);
        indicator_host.setText("Host: "+tarip);
        switchCam = true;
        TextView spd = findViewById(R.id.Indicator_speed);

    }

    public void disconnect_onclick(View view) {

        switchCam = false;
    }
}


