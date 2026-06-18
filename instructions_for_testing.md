Quick-Start Guide
1. Run Mosquitto
bash

cd edge-service
docker run -it -p 1883:1883 -v ./mosquitto.conf:/mosquitto/config/mosquitto.conf eclipse-mosquitto:2
2. Run Edge Service
bash

cd edge-service
npm install
cp .env.example .env   # edit MQTT_BROKER_URL, BACKEND_BASE_URL
npm run dev
3. Flash ESP32S3
Install PlatformIO IDE
Open arduino/nexbell_firmware/ folder
Edit src/config/Config.h with your Wi-Fi credentials and broker IP
pio run --target upload
4. Run Spring Boot Backend
Ensure MySQL is running, then run NexoraWebServiceApplication.java from IntelliJ.


1. Start the Docker Stack
Open your terminal in the edge-service directory and run:

powershell


cd c:\Users\krakentm\IdeaProjects\nexora_web_service\edge-service
docker compose up --build
This builds and starts the Edge Service and spins up a local Mosquitto Broker (exposing port 1883 on your host).

2. Verify the MQTT Broker
Since Mosquitto runs inside a container but maps its port to your host machine (1883:1883), you can connect any MQTT client directly to localhost:1883.

You can download MQTT Explorer (highly recommended GUI) and point it to localhost:1883 to watch the messages in real time.
3. Run Your Spring Boot Backend
Start your Java application (localhost:8080) from IntelliJ.

4. Perform the 3 Core Tests
The walkthrough guide details how to execute these tests:

Test 1: Presence Detection (ESP32S3 → Edge → Backend) Use the Docker container's broker to publish a presence change and verify that the backend prints a request trace or saves it in the database.
Test 2: Magnetic Door Alarms (ESP32S3 → Edge → Backend) Simulate a door opening (OPEN) or closing (CLOSED) and check Swagger (http://localhost:8080/swagger-ui.html) to verify that the tampering alarm was recorded in the database.
Test 3: Unlock Command (Backend → Edge → MQTT) Trigger an unlock request from the backend to verify that it reaches the Edge Service HTTP server on port 3100, which then publishes the UNLOCK command to the broker.
