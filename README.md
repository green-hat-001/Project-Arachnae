# Project-Arachnae
A semi-opensource prosthetic aimed at making prosthetics more usable and accessable to people with a tight budget and/or in developing countries. This project was started as passion project to experiment on a freind of mine who doesn't have an arm. Then as the project developed it became a vision to make these prosthetic more accessable and more usable compared to the super expensive ones out there that dont really have much functionalty built into them. 

The main step file has been compressed due to its size of CAD model. Below is the link to veiw the file in the browser as again the file is way too big to upload to github.
-> https://a360.co/4daP7HM

**How to use:**
- The user will wear a backplate that houses the arm. When the user turns on the arm a preloaded script automatically runs. Then the user can voice out commands like, "Top right cup" and the ML model built into the arm will use the camera mounted on the hand to search for a cup in the "top right" of the user. The camera also serves as a method for the hand to determine the optimal grip for the object.

- <img width="2145" height="1792" alt="1770411682987 (1)" src="https://github.com/user-attachments/assets/ebedc42d-b5cb-498f-b203-7d4289d15b30" />

The below will explain how the arm works, for greater detail contact me via the email (gal1le02620@gmail.com)

---

## How It Works: Technical Architecture

Project Arachnae leverages accessible hardware and software to create a highly functional, low-cost bionic arm. The system is broken down into three main categories: mechanical design, hardware, and the control system.

### 1. Mechanical Design & Manufacturing

To keep costs low and ensure the prosthetic is accessible globally, the structural components are fully 3D printable. 
* **Modeling:** The components are designed using CAD software like Fusion 360 and OpenSCAD. 
* **Fabrication:** Parts can be printed using standard, widely available filaments. Because the main CAD models (like the compressed STEP file mentioned above) are provided, users can modify the design to fit specific anatomical dimensions or hardware availability.

### 2. Hardware & Actuation
The physical movement of the arm relies on a hybrid motor setup designed to balance heavy lifting power with fine motor control:
* **Stepper Motor:** A NEMA 23 stepper motor is utilized for the joints requiring higher torque and structural stability. 
* **Servos:** Micro-servos actuate the finer movements, driving the tension systems that allow the individual fingers to flex and extend. 3xDS3218 are used to move each arm segments. However, V2 will house a ST3215 for better feedback and controllability and much higher torque ceilings. These servos are assisted through gravity compensation.

### 3. Electronics & Software Control
The "brain" of Project Arachnae uses a combination of high-level programming and low-level microcontroller execution:
* **Microcontroller:** An Arduino serves as the hardware interface. It is responsible for sending precise electrical signals to the motor drivers.
* **Logic & Processing
g:** Python runs in tandem with the Arduino to handle the heavier logic, process user inputs, and dictate the overall state of the arm.
* **Safety & Calibration:** To protect both the user and the 3D-printed mechanisms, the control code features a soft-start routine. The software explicitly prevents the servos from jumping violently to a default position upon startup, ensuring smooth, safe initialization. This will be improved in V2 thanks to a feature of the ST3215 that allows the user to read its angle at all times. 

### 4. BOM
## Bill of Materials (BOM)

| Item | Quantity | Description / Purpose | Link |
| :--- | :---: | :--- | :--- |
| DS3218 20kg Servo | 3 | High-torque servos for primary high-tension movements (e.g. main grip/wrist) | [Buy](https://www.amazon.ae/Miuzei-Torque-Waterproof-Control-Digital/dp/B0CQX9SYP2/ref=sr_1_5) |
| MG996R Pro Servo | 3 | Standard-torque metal gear servos for secondary finger articulation | [Buy](https://www.amazon.ae/Deegoo-4-Pack-MG996R-Digital-Helicopter/dp/B07MFK266B/ref=sr_1_2) |
| NEMA 23 Stepper Motor | 1 | High-torque motor for the main structural load-bearing joint | [Buy](https://www.amazon.ae/STEPPERONLINE-Stepper-Motor-339-79oz-57x57x82mm/dp/B091C37FJ2/ref=sr_1_3) |
| Arduino Uno (or Clone) | 1 | Main microcontroller for processing logic and sending motor signals | [Buy](https://www.amazon.ae/Arduino-bridges-high-performance-computing-real-time/dp/B0GFN669S4/ref=sr_1_16) |
| TB6600 Stepper Motor Driver | 1 | Required to safely deliver high current to the NEMA 23 | [Buy](https://www.amazon.ae/Usongshine-Stepper-Driver-Controller-Motor%EF%BC%881/dp/B07HHS14VQ/ref=sr_1_3) |
| 12V or 24V Power Supply | 1 | High-wattage wall adapter or PSU to power the stepper motor | [Buy](https://www.amazon.ae/Switching-Regulated-Transformer-Converter-Computer/dp/B09X9R9668/ref=sr_1_8) |
| 5V DC-DC Buck Converter | 3–4 | Steps down 12V/24V to a safe 5V for the servos (do not power servos directly from Arduino) | [Buy](https://www.amazon.ae/1-25-36V-Step-Down-Converter-Voltage-Regulator/dp/B0F24ZHB4S/ref=sr_1_3) |
| Long Strong Rubber Bands | 1 roll | Used as high-tension tendons for finger actuation | [Buy](https://www.amazon.ae/Kowvowz-Large-Rubber-Bands-Approximately/dp/B0CQ7F6KJS/ref=sr_1_37) |
| Jumper Wires & Perfboard | 1 set | For routing signals between the Arduino, driver, and servos | Local store (~30 AED) |
| M3 / M4 Screws & Nuts | 1 kit | Standard metric hardware for assembling the 3D printed chassis | Local store (~25 AED) |
| 3D Printer Filament (PLA+/PETG) | 3 kg | Main structural material — PLA+ or PETG recommended for durability | [Buy](https://www.amazon.ae/ELEGOO-Filament-Black-Dimensional-1kg/dp/B0F7LBXB2X/ref=sr_1_1_sspa) |
