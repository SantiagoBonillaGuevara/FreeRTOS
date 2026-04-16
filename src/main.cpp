#include <Arduino.h>

// ----------- Config -----------
#define TOUCH1 T0  // GPIO 4
#define TOUCH2 T7  // GPIO 27

#define QUEUE_LENGTH 10

// ----------- Struct de datos -----------
typedef struct {
  int sensor_id;
  int value;
  uint32_t timestamp;
} SensorData;

// ----------- Recursos RTOS -----------
QueueHandle_t queue0;
QueueHandle_t queue1;
SemaphoreHandle_t serialMutex;

// ----------- ISR flags -----------
volatile bool touch1detected = false;
volatile bool touch2detected = false;

// ----------- ISR -----------
void gotTouch1() { touch1detected = true; }
void gotTouch2() { touch2detected = true; }

// ----------- Función A (lectura sensor) -----------
void TaskSensor(void *pvParameters) {
  int sensor_id = (int)pvParameters;
  QueueHandle_t queue = (sensor_id == 0) ? queue0 : queue1;

  for (;;) {
    bool detected = false;

    if (sensor_id == 0 && touch1detected) {
      touch1detected = false;
      detected = true;
    }

    if (sensor_id == 1 && touch2detected) {
      touch2detected = false;
      detected = true;
    }

    if (detected) {
      SensorData data;
      data.sensor_id = sensor_id;
      data.value = 1;
      data.timestamp = millis();

      xQueueSend(queue, &data, portMAX_DELAY);
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ----------- Función B (envío serial) -----------
void TaskSender(void *pvParameters) {
  int sensor_id = (int)pvParameters;
  QueueHandle_t queue = (sensor_id == 0) ? queue0 : queue1;

  SensorData data;

  for (;;) {
    if (xQueueReceive(queue, &data, portMAX_DELAY)) {

      // 🔒 Tomar mutex antes de usar Serial
      xSemaphoreTake(serialMutex, portMAX_DELAY);

      Serial.print("{\"sensor\":");
      Serial.print(data.sensor_id);
      Serial.print(",\"value\":");
      Serial.print(data.value);
      Serial.print(",\"timestamp\":");
      Serial.print(data.timestamp);
      Serial.println("}");

      // 🔓 Liberar mutex
      xSemaphoreGive(serialMutex);
    }
  }
}

// ----------- Setup -----------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Touch (sin cables)
  touchAttachInterrupt(TOUCH1, gotTouch1, 40);
  touchAttachInterrupt(TOUCH2, gotTouch2, 40);

  // Crear colas
  queue0 = xQueueCreate(QUEUE_LENGTH, sizeof(SensorData));
  queue1 = xQueueCreate(QUEUE_LENGTH, sizeof(SensorData));

  // Crear mutex
  serialMutex = xSemaphoreCreateMutex();

  // Crear tareas (reutilizando funciones)
  xTaskCreate(TaskSensor, "Sensor0", 2048, (void *)0, 2, NULL);
  xTaskCreate(TaskSensor, "Sensor1", 2048, (void *)1, 2, NULL);

  xTaskCreate(TaskSender, "Sender0", 2048, (void *)0, 1, NULL);
  xTaskCreate(TaskSender, "Sender1", 2048, (void *)1, 1, NULL);

  Serial.println("Sistema iniciado");
}

void loop() {
  // No se usa (todo es RTOS)
}