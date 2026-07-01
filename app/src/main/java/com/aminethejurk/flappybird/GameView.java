package com.aminethejurk.flappybird;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.media.SoundPool;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class GameView extends SurfaceView implements Runnable {
    private Thread thread;
    private volatile boolean isPlaying;
    private SurfaceHolder surfaceHolder;
    private Paint paint;

    private int screenX, screenY;
    private float scaleX, scaleY;
    private float birdY, birdVelocity;
    private float birdRotation;
    private int score = 0;
    private boolean isMuted = false;

    private enum GameState { START, PLAYING, GAMEOVER }
    private GameState currentState = GameState.START;

    // Fixed Virtual Resolution matching Windows
    private static final float REF_WIDTH = 288f;
    private static final float REF_HEIGHT = 512f;
    
    // Constants EXACTLY matching the Windows version
    private static final float GRAVITY = 0.4f;
    private static final float JUMP_STRENGTH = -6.5f;
    private static final int PIPE_GAP = 130;
    private static final int PIPE_WIDTH = 52;
    private static final float GAME_SPEED = 3.0f;

    private Bitmap imgBird, imgBackground, imgPipe, imgGetReady, imgGameOver;
    private Bitmap[] imgDigits = new Bitmap[10];
    private SoundPool soundPool;
    private int soundJump, soundDeath;
    private MediaPlayer mediaPlayer;

    private List<Pipe> pipes = new ArrayList<>();
    private Random random = new Random();
    
    private RectF drawRect = new RectF();
    private android.graphics.Rect srcRect = new android.graphics.Rect();
    private Matrix birdMatrix = new Matrix();

    // Reusable Rects for collision to avoid GC
    private RectF birdRectObj = new RectF();
    private RectF topPipeRectObj = new RectF();
    private RectF botPipeRectObj = new RectF();

    private static class Pipe {
        float x, gapY;
        boolean passed = false;
        Pipe(float x, float gapY) { this.x = x; this.gapY = gapY; }
    }

    public GameView(Context context) {
        super(context);
        surfaceHolder = getHolder();
        paint = new Paint();
        paint.setAntiAlias(false);
        paint.setFilterBitmap(false); 

        loadAssets();
        initAudio();
    }

    private void loadAssets() {
        try {
            imgBackground = loadBitmap("Background/Background1.png");
            imgPipe = loadBitmap("Tiles/Style 1/PipeStyle1.png");
            imgBird = loadBitmap("Player/StyleBird2/birdalone.png");
            imgGetReady = loadBitmap("UI Messages/textGetReady.png");
            imgGameOver = loadBitmap("UI Messages/textGameOver.png");
            for (int i = 0; i < 10; i++) {
                imgDigits[i] = loadBitmap("Numbers/number" + i + ".png");
            }
        } catch (IOException e) { e.printStackTrace(); }
    }

    private Bitmap loadBitmap(String path) throws IOException {
        InputStream is = getContext().getAssets().open(path);
        return BitmapFactory.decodeStream(is);
    }

    private void initAudio() {
        AudioAttributes audioAttributes = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build();
        soundPool = new SoundPool.Builder().setMaxStreams(5).setAudioAttributes(audioAttributes).build();

        try {
            soundJump = soundPool.load(getContext().getAssets().openFd("Sounds/jump.wav"), 1);
            soundDeath = soundPool.load(getContext().getAssets().openFd("Sounds/death.wav"), 1);
            mediaPlayer = new MediaPlayer();
            mediaPlayer.setDataSource(getContext().getAssets().openFd("Sounds/backgroundsound.mp3").getFileDescriptor(),
                    getContext().getAssets().openFd("Sounds/backgroundsound.mp3").getStartOffset(),
                    getContext().getAssets().openFd("Sounds/backgroundsound.mp3").getLength());
            mediaPlayer.setLooping(true);
            mediaPlayer.prepare();
        } catch (IOException e) { e.printStackTrace(); }
    }

    @Override
    public void run() {
        while (isPlaying) {
            long frameStart = System.currentTimeMillis();
            update();
            draw();
            long frameTime = System.currentTimeMillis() - frameStart;
            if (frameTime < 16) {
                try { Thread.sleep(16 - frameTime); } catch (InterruptedException e) {}
            }
        }
    }

    private void update() {
        if (screenX == 0) return;
        scaleX = (float)screenX / REF_WIDTH;
        scaleY = (float)screenY / REF_HEIGHT;

        if (currentState == GameState.PLAYING) {
            birdVelocity += GRAVITY;
            birdY += birdVelocity;
            birdRotation = birdVelocity * 3.0f;
            if (birdRotation > 90) birdRotation = 90;
            if (birdRotation < -20) birdRotation = -20;

            for (int i = 0; i < pipes.size(); i++) {
                Pipe p = pipes.get(i);
                p.x -= GAME_SPEED;

                // Hitbox EXACTLY matching Windows: RectF birdRect(50, birdY - 10, 25, 20)
                birdRectObj.set(50, birdY - 10, 50 + 25, birdY - 10 + 20);
                topPipeRectObj.set(p.x, 0, p.x + PIPE_WIDTH, p.gapY - PIPE_GAP / 2f);
                botPipeRectObj.set(p.x, p.gapY + PIPE_GAP / 2f, p.x + PIPE_WIDTH, REF_HEIGHT);

                if (RectF.intersects(birdRectObj, topPipeRectObj) || RectF.intersects(birdRectObj, botPipeRectObj)) {
                    gameOver();
                }

                if (!p.passed && p.x < 50) {
                    p.passed = true;
                    score++;
                }
            }

            if (pipes.isEmpty() || pipes.get(pipes.size() - 1).x < REF_WIDTH - 160) {
                spawnPipe();
            }
            if (!pipes.isEmpty() && pipes.get(0).x < -PIPE_WIDTH) {
                pipes.remove(0);
            }

            if (birdY > REF_HEIGHT || birdY < 0) gameOver();
        }
    }

    private void spawnPipe() {
        pipes.add(new Pipe(REF_WIDTH, random.nextInt(200) + 150));
    }

    private void gameOver() {
        currentState = GameState.GAMEOVER;
        if (!isMuted) soundPool.play(soundDeath, 1, 1, 0, 0, 1);
    }

    private void draw() {
        if (surfaceHolder.getSurface().isValid()) {
            Canvas canvas = surfaceHolder.lockCanvas();
            if (canvas == null) return;
            
            screenX = canvas.getWidth();
            screenY = canvas.getHeight();
            scaleX = (float)screenX / REF_WIDTH;
            scaleY = (float)screenY / REF_HEIGHT;

            // 1. Background
            drawRect.set(0, 0, screenX, screenY);
            canvas.drawBitmap(imgBackground, null, drawRect, paint);

            // 2. Pipes
            for (Pipe p : pipes) {
                float topH = p.gapY - PIPE_GAP / 2.0f;
                float botStart = p.gapY + PIPE_GAP / 2.0f;
                // Top
                srcRect.set(0, 32, 32, 64);
                drawRect.set(p.x * scaleX, 0, (p.x + PIPE_WIDTH) * scaleX, (topH - 20) * scaleY);
                canvas.drawBitmap(imgPipe, srcRect, drawRect, paint);
                srcRect.set(0, 0, 32, 32);
                drawRect.set(p.x * scaleX, (topH - 20) * scaleY, (p.x + PIPE_WIDTH) * scaleX, topH * scaleY);
                canvas.drawBitmap(imgPipe, srcRect, drawRect, paint);
                // Bottom
                srcRect.set(0, 0, 32, 32);
                drawRect.set(p.x * scaleX, botStart * scaleY, (p.x + PIPE_WIDTH) * scaleX, (botStart + 20) * scaleY);
                canvas.drawBitmap(imgPipe, srcRect, drawRect, paint);
                srcRect.set(0, 32, 32, 64);
                drawRect.set(p.x * scaleX, (botStart + 20) * scaleY, (p.x + PIPE_WIDTH) * scaleX, screenY);
                canvas.drawBitmap(imgPipe, srcRect, drawRect, paint);
            }

            // 3. Bird
            birdMatrix.reset();
            birdMatrix.postTranslate(-imgBird.getWidth() / 2f, -imgBird.getHeight() / 2f);
            birdMatrix.postRotate(birdRotation);
            float birdDrawSize = 30f * scaleX; // Match Windows 30x30
            birdMatrix.postScale(birdDrawSize / imgBird.getWidth(), birdDrawSize / imgBird.getHeight());
            birdMatrix.postTranslate(67f * scaleX, birdY * scaleY);
            canvas.drawBitmap(imgBird, birdMatrix, paint);

            // 4. UI
            if (currentState == GameState.START) {
                drawRect.set((REF_WIDTH/2 - 92)*scaleX, 150*scaleY, (REF_WIDTH/2 + 92)*scaleX, 200*scaleY);
                canvas.drawBitmap(imgGetReady, null, drawRect, paint);
                paint.setColor(Color.WHITE); paint.setTextSize(14 * scaleX);
                paint.setTextAlign(Paint.Align.CENTER);
                canvas.drawText(isMuted ? "UNMUTE" : "MUTE", (REF_WIDTH - 40) * scaleX, (REF_HEIGHT - 20) * scaleY, paint);
            } else if (currentState == GameState.GAMEOVER) {
                drawRect.set((REF_WIDTH/2 - 96)*scaleX, 150*scaleY, (REF_WIDTH/2 + 96)*scaleX, 192*scaleY);
                canvas.drawBitmap(imgGameOver, null, drawRect, paint);
            }

            // 5. Score
            String s = String.valueOf(score);
            float dw = 24 * scaleX;
            float tw = s.length() * dw;
            for (int i = 0; i < s.length(); i++) {
                int digit = s.charAt(i) - '0';
                drawRect.set((REF_WIDTH/2f - tw/2f + i*dw), 50*scaleY, (REF_WIDTH/2f - tw/2f + (i+1)*dw), 86*scaleY);
                canvas.drawBitmap(imgDigits[digit], null, drawRect, paint);
            }

            surfaceHolder.unlockCanvasAndPost(canvas);
        }
    }

    public void resume() {
        isPlaying = true;
        thread = new Thread(this);
        thread.start();
        if (mediaPlayer != null && !isMuted) mediaPlayer.start();
    }

    public void pause() {
        isPlaying = false;
        try { if(thread != null) thread.join(); } catch (Exception e) {}
        if (mediaPlayer != null) mediaPlayer.pause();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            if (currentState == GameState.START && event.getX() > (REF_WIDTH - 70) * scaleX && event.getY() > (REF_HEIGHT - 40) * scaleY) {
                isMuted = !isMuted;
                if (isMuted) mediaPlayer.pause(); else mediaPlayer.start();
                return true;
            }
            if (currentState != GameState.PLAYING) {
                if (currentState == GameState.GAMEOVER) resetGame();
                currentState = GameState.PLAYING;
                if (mediaPlayer != null && !isMuted) mediaPlayer.start();
            }
            birdVelocity = JUMP_STRENGTH;
            if (!isMuted) soundPool.play(soundJump, 1, 1, 0, 0, 1);
        }
        return true;
    }

    private void resetGame() {
        birdY = REF_HEIGHT / 2f; birdVelocity = 0; score = 0;
        pipes.clear(); spawnPipe();
    }
}
