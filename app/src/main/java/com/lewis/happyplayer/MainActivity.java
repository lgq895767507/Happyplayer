package com.lewis.happyplayer;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity implements SeekBar.OnSeekBarChangeListener {

    private static final String TAG = "MainActivity";

    private SurfaceView surfaceView;
    private PlayerNative playerNative;
    private SeekBar seekBar;
    private int duration;//总时长
    private boolean isTouch;//设置是否正点击进度条
    private boolean isSeek;//设置是否正在拖动进度条


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        initView();

        playerNative = new PlayerNative();
        playerNative.setSurfaceView(surfaceView);
        playerNative.setDataSource(new File(
                Environment.getExternalStorageDirectory() + File.separator + "demo.mp4").getAbsolutePath());
        playerNative.setOnpreparedListener(new PlayerNative.OnpreparedListener() {
            @Override
            public void onPrepared() {
                //获取进度总时长
                duration = playerNative.getDuration();
                Log.e(TAG, "duration:" + duration);

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Log.e(TAG, "开始播放");
                        Toast.makeText(MainActivity.this, "开始播放！", Toast.LENGTH_SHORT).show();
                    }
                });
                //播放 调用到native去
                //start play
                playerNative.start();

            }
        });

        playerNative.setOnProgressListener(new PlayerNative.OnProgressListener() {
            @Override
            public void onProgress(final int progress) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (!isTouch) {
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    if (duration != 0) {
                                        if (isSeek) {
                                            isSeek = false;
                                            return;
                                        }
                                        seekBar.setProgress(progress * 100 / duration);
                                    }
                                }
                            });
                        }
                    }
                });
            }
        });

        playerNative.setOnErrorListener(new PlayerNative.OnErrorListener() {
            @Override
            public void onError(final int errorCode) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(MainActivity.this, "出错了，错误码：" + errorCode,
                                Toast.LENGTH_SHORT).show();
                    }
                });
            }
        });
    }

    private void initView() {
        surfaceView = findViewById(R.id.surfaceView);
        seekBar = findViewById(R.id.seek_bar);

        seekBar.setOnSeekBarChangeListener(this);
        findViewById(R.id.pause_btn).setOnClickListener(btnOnclickListener);
        findViewById(R.id.resume_btn).setOnClickListener(btnOnclickListener);
    }

    @Override
    protected void onResume() {
        super.onResume();
        playerNative.prepare();

    }

    @Override
    protected void onStop() {
        super.onStop();
        playerNative.stop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        playerNative.release();
    }


    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        isSeek = true;
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        isTouch = true;
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        isTouch = false;
        //获取seekbar的当前进度（百分比）
        int seekBarProgress = seekBar.getProgress();
        //将seekbar的进度转换成真实的播放进度
        int playProgress = seekBarProgress * duration / 100;
        //将播放进度传给底层 ffmpeg

        //seek 的核心思路2
        // 手动拖动进度条，要能跳到指定的播放进度  av_seek_frame
        playerNative.seekTo(playProgress);
    }

    private View.OnClickListener btnOnclickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            switch (v.getId()){
                case R.id.pause_btn:
                    playerNative.pause();
                    break;
                case R.id.resume_btn:
                    playerNative.resume();
                    break;
            }
        }
    };
}
