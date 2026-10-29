package com.juanmote.masterofdarkness;

import android.content.Intent;
import android.net.Uri;

import org.libsdl.app.SDLActivity;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class GameActivity extends SDLActivity {
    private static final int MSG_PICK_PATCH = 0x8001;
    private static final int MSG_PICK_MUSIC = 0x8002;
    private static final int PICK_PATCH = 2;
    private static final int PICK_MUSIC = 3;

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    /** Messages sent from the game with SDL_AndroidSendMessage. */
    @Override
    protected boolean onUnhandledMessage(int command, Object param) {
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
