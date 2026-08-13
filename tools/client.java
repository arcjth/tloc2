import java.io.InputStream;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class client {

    private static final String ESP32_IP = "192.168.4.1"; 
    private static final int DBG_PORT = 3333;             
    private static final int DBG_MAGIC = 0xBEEF1234; 

    private static final int PACKET_SIZE = 224; 

    public static void main(String[] args) {
        System.out.println("connecting to esp32  @" + ESP32_IP + ":" + DBG_PORT + "...");

        ByteBuffer streamBuffer = ByteBuffer.allocate(8192);
        streamBuffer.order(ByteOrder.LITTLE_ENDIAN);

        byte[] tempReadBuffer = new byte[2048];

        try (Socket socket = new Socket(ESP32_IP, DBG_PORT);
             InputStream inputStream = socket.getInputStream()) {

            System.out.println("successfully connected, streaming data\n");

            while (true) {
                int bytesRead = inputStream.read(tempReadBuffer);
                if (bytesRead == -1) {
                    System.out.println("\nclosed the connection.");
                    break;
                }

                System.out.printf("[RAW] got %d bytes from esp32.%n", bytesRead);
                streamBuffer.put(tempReadBuffer, 0, bytesRead);
                
                streamBuffer.flip();

                System.out.printf(" buffer state: remaining=%d, target_packet_size=%d%n",  streamBuffer.remaining(), PACKET_SIZE);

                while (streamBuffer.remaining() >= PACKET_SIZE) {
                    streamBuffer.mark();
                    
                    int magicCheck = streamBuffer.getInt();

                    if (magicCheck == DBG_MAGIC) {
                        int flags = streamBuffer.getInt();
                        
                        float ema0 = streamBuffer.getFloat();
                        float ema1 = streamBuffer.getFloat();
                        float ema2 = streamBuffer.getFloat();
                        float ema3 = streamBuffer.getFloat();
                        
                        float rUnit0 = streamBuffer.getFloat();
                        float rUnit1 = streamBuffer.getFloat();
                        float rUnit2 = streamBuffer.getFloat();
                        
                        float locX = streamBuffer.getFloat();
                        float locY = streamBuffer.getFloat();
                        float locDRef = streamBuffer.getFloat();

                        System.out.printf(" [PACKET] flags: %s |  coords: (%.2f, %.2f) |  d_ref: %.2f%n", Integer.toBinaryString(flags), locX, locY, locDRef);

                        System.out.printf(" EMA Mics: [%.1f, %.1f, %.1f, %.1f]%n", ema0, ema1, ema2, ema3);

                    } else {
                        System.out.printf(" [MISMATCH] expected Magic 0x%X, but got 0x%X. Skipping 1 byte...%n",  DBG_MAGIC, magicCheck);
                        streamBuffer.reset();
                        streamBuffer.get(); // trashes 1 byte
                    }
                }

                streamBuffer.compact();
            }

        } catch (Exception e) {
            System.err.println("Network Error: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
