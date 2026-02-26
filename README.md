# Project-Arachnae
A semi-opensource prosthetic aimed at making prosthetics more usable and accessable to people with a tight budget and/or in developing countries.

The main step file has been compressed due to its size of CAD model.

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
* **Logic & Processing:** Python runs in tandem with the Arduino to handle the heavier logic, process user inputs, and dictate the overall state of the arm.
* **Safety & Calibration:** To protect both the user and the 3D-printed mechanisms, the control code features a soft-start routine. The software explicitly prevents the servos from jumping violently to a default position upon startup, ensuring smooth, safe initialization. This will be improved in V2 thanks to a feature of the ST3215 that allows the user to read its angle at all times. 
