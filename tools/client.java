import java.io.InputStream;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class EspTelemetryClient {

    private static final String ESP32_IP = "192.168.4.1"; 
    private static final int DBG_PORT = 3333;             
    private static final int DBG_MAGIC = 0xABCD1234; 

    private static final int PACKET_SIZE = 48; 

    public static void main(String[] args) {
        System.out.println("Connecting to ESP32 stream at " + ESP32_IP + ":" + DBG_PORT + "...");

        ByteBuffer streamBuffer = ByteBuffer.allocate(8192);
        streamBuffer.order(ByteOrder.LITTLE_ENDIAN);

        byte[] tempReadBuffer = new byte[2048];

        // Automatic resource management handles closing the socket
        try (Socket socket = new Socket(ESP32_IP, DBG_PORT);
             InputStream inputStream = socket.getInputStream()) {

            System.out.println("Connected successfully! Streaming data...\n");

            while (true) {
                // 1. Read raw chunks from the Wi-Fi network
                int bytesRead = inputStream.read(tempReadBuffer);
                if (bytesRead == -1) {
                    System.out.println("\nESP32 closed the connection.");
                    break;
                }

                // 2. Feed the raw bytes into our processing ByteBuffer
                streamBuffer.put(tempReadBuffer, 0, bytesRead);
                
                // Flip the buffer to prepare it for reading/parsing
                streamBuffer.flip();

                // 3. Process as many packets as we can pull out of the stream
                while (streamBuffer.remaining() >= PACKET_SIZE) {
                    // Mark our current position in case the magic number check fails
                    streamBuffer.mark();
                    
                    int magicCheck = streamBuffer.getInt();

                    if (magicCheck == DBG_MAGIC) {
                        // Success! We found the start of a packet. Extract variables sequentially.
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

                        // Print the telemetry data cleanly
                        System.out.printf("[PACKET] Flags: %s | Coords: (%.2f, %.2f) | d_ref: %.2f%n",
                                Integer.toBinaryString(flags), locX, locY, locDRef);
                        System.out.printf("         EMA Mics: [%.1f, %.1f, %.1f, %.1f]%n", 
                                ema0, ema1, ema2, ema3);

                    } else {
                        // Stream is misaligned. Rewind to our mark, skip 1 single byte, and look again.
                        streamBuffer.reset();
                        streamBuffer.get(); // Discard 1 byte
                    }
                }

                // Compact the buffer to shift any partial packet remnants to the beginning 
                // and clear up space for the next network read loop
                streamBuffer.compact();
            }

        } catch (Exception e) {
            System.err.println("Network Error: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
