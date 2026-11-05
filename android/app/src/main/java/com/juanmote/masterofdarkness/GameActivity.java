package com.juanmote.masterofdarkness;

import android.content.Context;
import android.content.Intent;
import android.media.AudioAttributes;
import android.net.Uri;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class GameActivity extends SDLActivity {
    private static final int MSG_PICK_PATCH = 0x8001;
    private static final int MSG_PICK_MUSIC = 0x8002;
    private static final int MSG_EXIT = 0x8003;
    private static final int MSG_VIBRATE = 0x8004;   // param: strength (1-255) << 16 | milliseconds
    private static final int MSG_EXPORT_PROFILE = 0x8005;
    private static final int MSG_IMPORT_PROFILE = 0x8006;
    private static final int MSG_PICK_PACK = 0x8007;   // param: 1 gallery, 2 manual
    private static final int MSG_EXPORT_STATE = 0x8008;
    private static final int EXPORT_PROFILE = 4;
    private static final int EXPORT_STATE = 7;
    private static final int PICK_PACK = 6;
    private static int packKind = 1;
    private static final int IMPORT_PROFILE = 5;
    private Vibrator vibrator;
    private static boolean exitRequested;
    private static final int PICK_PATCH = 2;
    private static final int PICK_MUSIC = 3;
    private static int musicSlot = 1;   // music pack slot being chosen (OST 1-3)

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    /** Messages sent from the game with SDL_AndroidSendMessage. */
    @Override
    protected boolean onUnhandledMessage(int command, Object param) {
        if (command == MSG_VIBRATE && param instanceof Integer) {
            vibrate((Integer) param);
            return true;
        }
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
        if (command == MSG_EXPORT_PROFILE || command == MSG_IMPORT_PROFILE) {
            final boolean export = command == MSG_EXPORT_PROFILE;
            runOnUiThread(new Runnable() {
                @Override public void run() {
                    Intent i = new Intent(export ? Intent.ACTION_CREATE_DOCUMENT : Intent.ACTION_OPEN_DOCUMENT);
                    i.addCategory(Intent.CATEGORY_OPENABLE);
                    i.setType(export ? "text/plain" : "*/*");
                    if (export) i.putExtra(Intent.EXTRA_TITLE, "MasterOfDarkness_perfil.txt");
                    startActivityForResult(i, export ? EXPORT_PROFILE : IMPORT_PROFILE);
                }
            });
            return true;
        }
        if (command == MSG_EXPORT_STATE) {
            runOnUiThread(new Runnable() {
                @Override public void run() {
                    Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                    i.addCategory(Intent.CATEGORY_OPENABLE);
                    i.setType("application/octet-stream");
                    i.putExtra(Intent.EXTRA_TITLE, "MasterOfDarkness_estado.sav");
                    startActivityForResult(i, EXPORT_STATE);
                }
            });
            return true;
        }
        if (command == MSG_PICK_PACK) {
            if (param instanceof Integer) packKind = (Integer) param;
            runOnUiThread(new Runnable() {
                @Override public void run() {
                    Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    i.addCategory(Intent.CATEGORY_OPENABLE);
                    i.setType("*/*");
                    startActivityForResult(i, PICK_PACK);
                }
            });
            return true;
        }
        if (command == MSG_PICK_PATCH || command == MSG_PICK_MUSIC) {
            final int request = command == MSG_PICK_PATCH ? PICK_PATCH : PICK_MUSIC;
            if (command == MSG_PICK_MUSIC && param instanceof Integer) musicSlot = (Integer) param;
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

    /** One vibration of the device's own motor, with its strength when the motor supports it. */
    @SuppressWarnings("deprecation")
    private void vibrate(int packed) {
        try {
            if (vibrator == null) vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
            if (vibrator == null || !vibrator.hasVibrator()) return;
            int amp = (packed >> 16) & 0xFF, ms = packed & 0xFFFF;
            if (amp < 1) amp = 1;
            if (ms < 1) return;
            if (Build.VERSION.SDK_INT >= 26) {
                AudioAttributes attr = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).build();
                int a = vibrator.hasAmplitudeControl() ? amp : VibrationEffect.DEFAULT_AMPLITUDE;
                vibrator.vibrate(VibrationEffect.createOneShot(ms, a), attr);
            } else {
                vibrator.vibrate(ms);
            }
        } catch (Exception e) {
            // no vibration on this device
        }
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
        if (requestCode == PICK_MUSIC) {
            String name = musicSlot == 2 ? "musica2.zip" : musicSlot == 3 ? "musica3.zip" : "musica.zip";
            copyPack(data.getData(), name);
            return;
        }
        if (requestCode == PICK_PACK) { copyPack(data.getData(), packKind == 2 ? "manual.zip" : "galeria.zip"); return; }
        if (requestCode == EXPORT_PROFILE) { exportProfile(data.getData()); return; }
        if (requestCode == EXPORT_STATE) { copyOut("estado1.sav", data.getData()); return; }
        if (requestCode == IMPORT_PROFILE) { importProfile(data.getData()); return; }
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

    /** A pack (a .zip, possibly large) is copied inside the app under the given name, without loading it in memory:
     *  musica.zip, musica2.zip, musica3.zip (OST 1-3), galeria.zip or manual.zip. */
    private void copyPack(Uri uri, String name) {
        File tmp = new File(getFilesDir(), "pack.tmp");
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
            File dst = new File(getFilesDir(), name);
            if (header) { dst.delete(); tmp.renameTo(dst); }
            else tmp.delete();
        } catch (Exception e) {
            tmp.delete();
        }
    }

    /** A file of the app (the saved state) goes to the file the player chose. */
    private void copyOut(String name, Uri uri) {
        boolean ok = false;
        try {
            FileInputStream in = new FileInputStream(new File(getFilesDir(), name));
            OutputStream out = getContentResolver().openOutputStream(uri, "wt");
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            in.close();
            out.close();
            ok = true;
        } catch (Exception e) {
            // told below
        }
        Toast.makeText(this, ok ? "OK" : "Error", Toast.LENGTH_SHORT).show();
    }

    /** The profile (logros.txt) goes to the file the player chose. */
    private void exportProfile(Uri uri) {
        boolean ok = false;
        try {
            File src = new File(getFilesDir(), "logros.txt");
            if (!src.exists()) new FileOutputStream(src, true).close();
            FileInputStream in = new FileInputStream(src);
            OutputStream out = getContentResolver().openOutputStream(uri, "wt");
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            in.close();
            out.close();
            ok = true;
        } catch (Exception e) {
            // told below
        }
        Toast.makeText(this, ok ? "OK" : "Error", Toast.LENGTH_SHORT).show();
    }

    /** The chosen file is left for the game, which checks it and adds it to the profile. */
    private void importProfile(Uri uri) {
        try {
            byte[] p = readAll(uri);
            if (p.length > 65536) p = new byte[0];   // a profile is tiny: the game will refuse this
            FileOutputStream out = new FileOutputStream(new File(getFilesDir(), "perfil_importado.txt"));
            out.write(p);
            out.close();
        } catch (Exception e) {
            // no file: nothing is imported
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
