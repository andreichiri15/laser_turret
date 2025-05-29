import com.fazecast.jSerialComm.SerialPort;

import javax.swing.*;
import java.awt.event.*;
import java.util.concurrent.*;

public class SerialMouseSender extends JFrame {
    private final SerialPort comPort;
    private int mouseX = 0, mouseY = 0;
    private boolean mouseHeld = false;
    private final ScheduledExecutorService tick =
        Executors.newSingleThreadScheduledExecutor();

    public SerialMouseSender(String port) {
        // --- Serial setup ---
        comPort = SerialPort.getCommPort(port);
        comPort.setBaudRate(9600);
        // non-blocking reads/writes
        comPort.setComPortTimeouts(
            SerialPort.TIMEOUT_NONBLOCKING,
            0,
            0
        );

        if (!comPort.openPort()) {
            JOptionPane.showMessageDialog(
                this,
                "Failed to open serial port: " + port,
                "Error",
                JOptionPane.ERROR_MESSAGE
            );
            System.exit(1);
        }

        // --- Drain thread ---
        Thread reader = new Thread(() -> {
            byte[] buffer = new byte[256];
            while (!Thread.currentThread().isInterrupted()) {
                int num = comPort.readBytes(buffer, buffer.length);
                // num <= 0 means no data; we just spin
                // if you want to debug incoming data, you could:
                if (num>0) System.out.println(new String(buffer,0,num));
                try {
                    Thread.sleep(10);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        }, "Serial-Drainer");
        reader.setDaemon(true);
        reader.start();

        // --- GUI setup ---
        setTitle("Serial Sender");
        setSize(800, 600);
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setLocationRelativeTo(null);
        setMouseTracking();

        // --- Schedule sends off the EDT ---
        tick.scheduleAtFixedRate(this::sendSerial, 0, 100, TimeUnit.MILLISECONDS);
    }

    private void setMouseTracking() {
        addMouseMotionListener(new MouseMotionAdapter() {
            public void mouseMoved(MouseEvent e) {
                mouseX = e.getX();
                mouseY = e.getY();
            }
            public void mouseDragged(MouseEvent e) {
                mouseX = e.getX();
                mouseY = e.getY();
            }
        });
        addMouseListener(new MouseAdapter() {
            public void mousePressed(MouseEvent e) {
                mouseHeld = true;
            }
            public void mouseReleased(MouseEvent e) {
                mouseHeld = false;
            }
        });
    }

    private void sendSerial() {
        String msg = (mouseHeld ? "C" : "")
                   + "X" + mouseX
                   + "Y" + mouseY
                   + "\n";  // keep the newline delimiter
        byte[] data = msg.getBytes();
        int written = comPort.writeBytes(data, data.length);
        if (written < 0) {
            System.err.println("Write error");
        }
    }

    public static void main(String[] args) {
        String port = (args.length > 0) ? args[0] : "COM3";
        SwingUtilities.invokeLater(() ->
            new SerialMouseSender(port).setVisible(true)
        );
    }
}
