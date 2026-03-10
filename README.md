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
