# FreeRTOS – Conceptos y Ejemplos

## 1. Ejecutar tareas con la misma función pero distintos parámetros

En FreeRTOS todas las tareas reciben un `void *pvParameters`. Puedes reutilizar la misma función pasando diferentes parámetros.

```c
void TaskExample(void *pvParameters) {
  int id = (int) pvParameters;

  for (;;) {
    Serial.print("Soy la tarea ");
    Serial.println(id);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  xTaskCreate(TaskExample, "T1", 2048, (void*)0, 1, NULL);
  xTaskCreate(TaskExample, "T2", 2048, (void*)1, 1, NULL);
}
```

> Ambas tareas ejecutan la misma función, pero con comportamiento distinto según `id`.

---

## 2. Tipo de dato de una tarea y cómo convertirlo

La firma de una tarea siempre recibe un puntero genérico:

```c
void Task(void *pvParameters)
```

### Casting simple

```c
int valor = (int) pvParameters;
```

### Casting estructurado (recomendado para sistemas reales)

```c
typedef struct {
  int id;
  int pin;
} Params;

void Task(void *pvParameters) {
  Params *p = (Params*) pvParameters;
  Serial.println(p->id);
}
```

---

## 3. ¿Qué pasa si una cola se llena?

Depende del timeout usado en `xQueueSend`:

| Modo | Código | Comportamiento |
|---|---|---|
| 🔴 Bloqueante | `xQueueSend(queue, &data, portMAX_DELAY)` | La tarea se bloquea hasta que haya espacio |
| 🟡 Timeout | `xQueueSend(queue, &data, 100 / portTICK_PERIOD_MS)` | Espera un tiempo limitado |
| 🟢 No bloqueante | `xQueueSend(queue, &data, 0)` | Retorna `errQUEUE_FULL` inmediatamente |

```c
if (xQueueSend(queue, &data, 0) == errQUEUE_FULL) {
  // manejar error
}
```

---

## 4. ¿Varias tareas pueden leer/escribir la misma cola?

**Sí.** FreeRTOS garantiza sincronización interna:

- Múltiples productores → ✅ OK
- Múltiples consumidores → ✅ OK

> Tener en cuenta que puede haber condiciones de lógica (no de memoria) y que el orden depende del scheduler.

---

## 5. Deadlock

Un **deadlock** ocurre cuando dos o más tareas quedan bloqueadas esperando recursos que nunca se liberan.

### Ejemplo de deadlock

```c
SemaphoreHandle_t mutexA;
SemaphoreHandle_t mutexB;

void Task1(void *pvParameters) {
  for (;;) {
    xSemaphoreTake(mutexA, portMAX_DELAY);
    vTaskDelay(100);
    xSemaphoreTake(mutexB, portMAX_DELAY); // Task1 tiene A, espera B

    xSemaphoreGive(mutexB);
    xSemaphoreGive(mutexA);
  }
}

void Task2(void *pvParameters) {
  for (;;) {
    xSemaphoreTake(mutexB, portMAX_DELAY);
    vTaskDelay(100);
    xSemaphoreTake(mutexA, portMAX_DELAY); // Task2 tiene B, espera A → DEADLOCK

    xSemaphoreGive(mutexA);
    xSemaphoreGive(mutexB);
  }
}
```

### ¿Por qué ocurre?

- `Task1` tiene `mutexA` y espera `mutexB`
- `Task2` tiene `mutexB` y espera `mutexA`
- Ninguna puede continuar

### Cómo evitarlo

- Usar un **orden consistente** al adquirir locks
- Usar **timeout** en los semáforos en lugar de `portMAX_DELAY`
- Evitar múltiples mutex innecesarios

### Diagrama

<img width="323" height="296" alt="deadlock" src="https://github.com/user-attachments/assets/d2b50d22-f517-4a64-9b10-6808bc49c181" />

---

## 6. Condición de carrera (sin mutex)

```c
int contador = 0;

void Task1(void *pvParameters) {
  for (;;) {
    contador++;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void Task2(void *pvParameters) {
  for (;;) {
    contador--;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
```

> ⚠️ Acceso concurrente a `contador` produce resultados inconsistentes.

---

## 7. Solución con mutex

```c
SemaphoreHandle_t mutex;
int contador = 0;

void Task1(void *pvParameters) {
  for (;;) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    contador++;
    xSemaphoreGive(mutex);

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void Task2(void *pvParameters) {
  for (;;) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    contador--;
    xSemaphoreGive(mutex);

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  mutex = xSemaphoreCreateMutex();
}
```

> ✅ Acceso exclusivo garantizado, sin corrupción de datos.

### Diagrama: condición de carrera vs solución con mutex

<img width="729" height="302" alt="accesoConcurrente" src="https://github.com/user-attachments/assets/ee6956a5-fcaf-4109-ad45-da23f7afd221" />

---

## 8. Diagrama del sistema de la práctica

<img width="329" height="488" alt="diagramaSistema" src="https://github.com/user-attachments/assets/4a4460ef-3afa-4aaf-b8b1-cee10bcc6488" />

### Explicación del Diagrama del Sistema (FreeRTOS con sensores touch)

Este diagrama representa la arquitectura del sistema implementado en el ESP32 utilizando FreeRTOS, donde se gestionan múltiples tareas concurrentes para la lectura de sensores táctiles y el envío de datos por el puerto serial.

#### 🔹 Componentes del sistema

El sistema está dividido en tres bloques principales:

---

#### 🟦 1. Tareas de adquisición (Productores)

- **TaskSensor 0 (TS0)**  
- **TaskSensor 1 (TS1)**  

Estas tareas se encargan de:
- Detectar eventos de los sensores táctiles (Touch T0 y T7).
- Generar datos que incluyen:
  - ID del sensor
  - Valor detectado
  - Timestamp (marca de tiempo)
- Enviar estos datos a una cola específica.

Cada tarea trabaja de forma independiente y utiliza la misma función, diferenciándose por parámetros.

---

#### 🟨 2. Colas (Queues)

- **Queue 0 (Q0)**  
- **Queue 1 (Q1)**  

Las colas actúan como buffers intermedios entre las tareas productoras y consumidoras:

- **TS0 → Q0**
- **TS1 → Q1**

Su función es:
- Almacenar temporalmente los datos generados por los sensores.
- Desacoplar la velocidad de producción y consumo.
- Evitar pérdida de datos si el consumidor está ocupado.

---

#### 🟩 3. Tareas de envío (Consumidores)

- **TaskSender 0 (TX0)**  
- **TaskSender 1 (TX1)**  

Estas tareas:
- Leen datos desde sus respectivas colas.
- Formatean la información en formato JSON.
- Intentan enviar los datos por el puerto serial.

Flujo:
- **Q0 → TX0**
- **Q1 → TX1**

---

#### 🔒 4. Mutex (Control de concurrencia)

- **Mutex Serial (M)**  

El mutex se utiliza para:
- Controlar el acceso concurrente al puerto serial.
- Evitar que múltiples tareas escriban al mismo tiempo y corrompan los datos.

Funcionamiento:
- Antes de escribir en Serial, una tarea debe adquirir el mutex.
- Al terminar, libera el mutex para que otra tarea pueda usarlo.

---

#### 🖥️ 5. Salida

- **Serial (S)**  

Es el recurso compartido donde:
- Se imprimen los datos en formato JSON.
- Representa el canal de salida del sistema (monitor serial o comunicación externa).

---

#### 🔄 Flujo completo del sistema

1. Un sensor táctil detecta una interacción.
2. La tarea `TaskSensor` correspondiente genera un dato.
3. El dato se envía a su cola asignada.
4. La tarea `TaskSender` lee el dato desde la cola.
5. La tarea solicita acceso al mutex.
6. Una vez obtenido, envía el dato por Serial.
7. Libera el mutex para permitir el acceso a otras tareas.

8. ### 🎯 Características clave del diseño

- **Concurrencia real** mediante múltiples tareas.
- **Desacoplamiento** usando colas (producer-consumer).
- **Seguridad en recursos compartidos** mediante mutex.
- **Reutilización de código** (mismas funciones para múltiples tareas).
- **Escalabilidad** (se pueden añadir más sensores fácilmente).

---

## 9. Diagrama de arquitectura
<img width="295" height="716" alt="diagramaArquitectura" src="https://github.com/user-attachments/assets/a42ab1c5-ed75-4550-a0b5-62b1cf7dd897" />
