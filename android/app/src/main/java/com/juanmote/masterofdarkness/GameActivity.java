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
    private static final int PICK_PATCH = 2;

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    /** Messages sent from the game with SDL_AndroidSendMessage. */
    @Override
    protected boolean onUnhandledMessage(int command, Object param) {
        if (command == MSG_PICK_PATCH) {
            runOnUiThread(new Runnable() {
                @Override public void run() {
                    Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    i.addCategory(Intent.CATEGORY_OPENABLE);
                    i.setType("*/*");
                    startActivityForResult(i, PICK_PATCH);
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
        if (requestCode != PICK_PATCH || resultCode != RESULT_OK || data == null || data.getData() == null) return;
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
