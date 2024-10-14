# Software side of the project

## Prerequisites

- Install the [Arduino IDE](https://www.arduino.cc/en/software) on your computer.
- Connect your Arduino Nano to the computer via a USB cable.

## Steps to Upload Code

1. **Open Arduino IDE:**
   Launch the Arduino IDE on your computer.

2. **Write or Open Your Code:**

   - If you're writing a new program, write the code in the main window.
   - If you already have a program, open it by going to `File > Open` and selecting your `.ino` file.

3. **Select Your Arduino Nano Board:**

   - Go to `Tools > Board` and select **Arduino Nano** from the list.

4. **Select the Correct Processor:**

   - Go to `Tools > Processor` and select the appropriate option based on your board version:
     - For older Nano models (pre-2018), choose **ATmega328P**.
     - For newer models, choose **ATmega328P (Old Bootloader)**.
   - If you’re unsure, try one option, and if you encounter upload issues, switch to the other.

5. **Select the Correct Port:**

   - Go to `Tools > Port` and select the port corresponding to your Arduino Nano. The port name may appear as `COMX` on Windows or `/dev/ttyUSBX` on Linux/Mac, where `X` is the port number.

6. **Verify the Code:**

   - Click the checkmark icon (✔️) on the top left of the IDE to verify the code. This will compile the code and check for errors.
   - If the code compiles successfully, you will see a message saying "Done compiling."

7. **Upload the Code:**

   - Click the arrow icon (➡️) next to the checkmark or go to `Sketch > Upload`. This will upload the code to your Arduino Nano board.
   - Wait for the "Done uploading" message in the output window of the IDE.

8. **Monitor the Serial Output (Optional):**
   - If your code includes `Serial.begin()` for serial communication, you can open the Serial Monitor by clicking the magnifying glass icon or going to `Tools > Serial Monitor`. This will allow you to view the output from the Arduino.

## Troubleshooting

- **Port not showing up:** Make sure the Arduino is properly connected, and check the USB cable and connection.
- **Upload Error (`avrdude: stk500_getsync()`):** Switch the `Processor` setting to **ATmega328P (Old Bootloader)** and try uploading again.
- **Error during upload:** Ensure you selected the correct board, processor, and port in the `Tools` menu. If you still have issues, try resetting the Arduino and restarting the Arduino IDE.
