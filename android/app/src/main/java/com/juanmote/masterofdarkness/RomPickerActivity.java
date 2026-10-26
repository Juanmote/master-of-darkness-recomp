package com.juanmote.masterofdarkness;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/** First screen: makes sure the user's ROM is installed, then starts the game. */
public class RomPickerActivity extends Activity {
    private static final int PICK_ROM = 1;
    private TextView status;

    private File romFile() { return new File(getFilesDir(), "rom.sms"); }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (romFile().exists()) {
            startGame();
            return;
        }
        buildUi();
    }

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setBackgroundColor(Color.rgb(16, 8, 8));
        root.setPadding(48, 48, 48, 48);

        TextView title = new TextView(this);
        title.setText("MASTER OF DARKNESS");
        title.setTextColor(Color.rgb(232, 192, 96));
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        status = new TextView(this);
        status.setText("Para jugar necesitas tu ROM de Master of Darkness para Master System (.sms).\n"
                + "Solo hay que elegirla una vez: se copiará dentro de la app.");
        status.setTextColor(Color.WHITE);
        status.setTextSize(16);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, 32, 0, 32);
        root.addView(status);

        Button pick = new Button(this);
        pick.setText("Elegir ROM");
        pick.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { openPicker(); }
        });
        root.addView(pick);
        setContentView(root);
        pick.requestFocus();
    }

    private void openPicker() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, PICK_ROM);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_ROM || resultCode != RESULT_OK || data == null || data.getData() == null) return;
        try {
            byte[] rom = readAll(data.getData());
            if (rom.length < 0x8000 || rom.length > (1 << 20) || !hasSegaHeader(rom)) {
                status.setText("Ese archivo no parece una ROM de Master System. Prueba con otro.");
                return;
            }
            FileOutputStream out = new FileOutputStream(romFile());
            out.write(rom);
            out.close();
            startGame();
        } catch (Exception e) {
            status.setText("No se pudo leer el archivo: " + e.getMessage());
        }
    }

    private byte[] readAll(Uri uri) throws Exception {
        InputStream in = getContentResolver().openInputStream(uri);
        ByteArrayOutputStream buf = new ByteArrayOutputStream();
        byte[] tmp = new byte[65536];
        int n;
        while ((n = in.read(tmp)) > 0) {
            buf.write(tmp, 0, n);
            if (buf.size() > (2 << 20)) break;
        }
        in.close();
        return buf.toByteArray();
    }

    private static boolean hasSegaHeader(byte[] rom) {
        byte[] sig = "TMR SEGA".getBytes();
        for (int i = 0; i < 8; i++) if (rom[0x7FF0 + i] != sig[i]) return false;
        return true;
    }

    private void startGame() {
        startActivity(new Intent(this, GameActivity.class));
        finish();
    }
}
