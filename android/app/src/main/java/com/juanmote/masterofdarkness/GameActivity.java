package com.juanmote.masterofdarkness;

import android.content.Intent;
import android.net.Uri;

import android.view.KeyEvent;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLControllerManager;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class GameActivity extends SDLActivity {
    private static final int MSG_PICK_PATCH = 0x8001;
    private static final int MSG_PICK_MUSIC = 0x8002;
    private static final int MSG_EXIT = 0x8003;
    private static boolean exitRequested;
    private static final int PICK_PATCH = 2;
    private static final int PICK_MUSIC = 3;

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    /** Messages sent from the game with SDL_AndroidSendMessage. */
    @Override
    protected boolean onUnhandledMessage(int command, Object param) {
        if (command == MSG_EXIT) {
            // "Exit": close the window, remove it from recents and end the process
            exitRequested = true;
            runOnUiThread(new Runnable() {
                @Override public void run() {
                    if (android.os.Build.VERSION.SDK_INT >= 21) finishAndRemoveTask(); else finish();
                }
            });
            return true;
        }
        if (command == MSG_PICK_PATCH || command == MSG_PICK_MUSIC) {
            final int request = command == MSG_PICK_PATCH ? PICK_PATCH : PICK_MUSIC;
            runOnUiThread(new Runnable() {
                @Override public void run() {
                    Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    i.addCategory(Intent.CATEGORY_OPENABLE);
                    i.setType("*/*");
                    startActivityForResult(i, request);
                }
            });
            return true;
        }
        return super.onUnhandledMessage(command, param);
    }

    /**
     * Gamepad buttons that SDL does not take as a controller (some handhelds send their built-in
     * controls as plain keys) would be lost: SDL has no keyboard key for them. They are passed to the
     * game as keys it understands: A jump, B/X attack, Y subweapon, START pause, SELECT menu,
     * L2 rewind, R2 fast forward.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int code = event.getKeyCode();
        int mapped = gamepadKey(code);
        int action = event.getAction();
        if (mapped != 0 && (action == KeyEvent.ACTION_DOWN || action == KeyEvent.ACTION_UP)) {
            int dev = event.getDeviceId();
            if (SDLControllerManager.isDeviceSDLJoystick(dev)) {
                int r = action == KeyEvent.ACTION_DOWN ? SDLControllerManager.onNativePadDown(dev, code)
                                                       : SDLControllerManager.onNativePadUp(dev, code);
                if (r == 0) return true;   // SDL read it as a controller
            }
            if (event.getRepeatCount() == 0 || action == KeyEvent.ACTION_UP) {
                if (action == KeyEvent.ACTION_DOWN) SDLActivity.onNativeKeyDown(mapped);
                else SDLActivity.onNativeKeyUp(mapped);
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    private static int gamepadKey(int code) {
        switch (code) {
            case KeyEvent.KEYCODE_BUTTON_A: return KeyEvent.KEYCODE_SPACE;
            case KeyEvent.KEYCODE_BUTTON_B: return KeyEvent.KEYCODE_J;
            case KeyEvent.KEYCODE_BUTTON_X: return KeyEvent.KEYCODE_J;
            case KeyEvent.KEYCODE_BUTTON_Y: return KeyEvent.KEYCODE_C;
            case KeyEvent.KEYCODE_BUTTON_START: return KeyEvent.KEYCODE_ENTER;
            case KeyEvent.KEYCODE_BUTTON_SELECT: return KeyEvent.KEYCODE_ESCAPE;
            case KeyEvent.KEYCODE_BUTTON_MODE: return KeyEvent.KEYCODE_ESCAPE;
            case KeyEvent.KEYCODE_BUTTON_L2: return KeyEvent.KEYCODE_Q;
            case KeyEvent.KEYCODE_BUTTON_R2: return KeyEvent.KEYCODE_E;
            case KeyEvent.KEYCODE_DPAD_UP: return KeyEvent.KEYCODE_DPAD_UP;
            case KeyEvent.KEYCODE_DPAD_DOWN: return KeyEvent.KEYCODE_DPAD_DOWN;
            case KeyEvent.KEYCODE_DPAD_LEFT: return KeyEvent.KEYCODE_DPAD_LEFT;
            case KeyEvent.KEYCODE_DPAD_RIGHT: return KeyEvent.KEYCODE_DPAD_RIGHT;
        }
        return 0;
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (exitRequested) System.exit(0);
    }

    /** The chosen translation patch (IPS) is copied inside the app as patch.ips. */
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        if (requestCode == PICK_MUSIC) { copyMusicPack(data.getData()); return; }
        if (requestCode != PICK_PATCH) return;
        try {
            byte[] p = readAll(data.getData());
            if (p.length < 8 || p[0] != 'P' || p[1] != 'A' || p[2] != 'T' || p[3] != 'C' || p[4] != 'H') return;
            FileOutputStream out = new FileOutputStream(new File(getFilesDir(), "patch.ips"));
            out.write(p);
            out.close();
        } catch (Exception e) {
            // the game shows that no patch was loaded
        }
    }

    /** The music pack (a .zip, possibly large) is copied as musica.zip without loading it in memory. */
    private void copyMusicPack(Uri uri) {
        File tmp = new File(getFilesDir(), "musica.tmp");
        try {
            InputStream in = getContentResolver().openInputStream(uri);
            FileOutputStream out = new FileOutputStream(tmp);
            byte[] buf = new byte[65536];
            int n, total = 0;
            boolean header = false;
            while ((n = in.read(buf)) > 0) {
                if (total == 0) header = n >= 2 && buf[0] == 'P' && buf[1] == 'K';
                out.write(buf, 0, n);
                total += n;
            }
            in.close();
            out.close();
            if (header) tmp.renameTo(new File(getFilesDir(), "musica.zip"));
            else tmp.delete();
        } catch (Exception e) {
            tmp.delete();
        }
    }

    private byte[] readAll(Uri uri) throws Exception {
        InputStream in = getContentResolver().openInputStream(uri);
        ByteArrayOutputStream buf = new ByteArrayOutputStream();
        byte[] tmp = new byte[65536];
        int n;
        while ((n = in.read(tmp)) > 0) {
            buf.write(tmp, 0, n);
            if (buf.size() > (4 << 20)) break;
        }
        in.close();
        return buf.toByteArray();
    }
}
