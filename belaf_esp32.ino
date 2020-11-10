#include <WiFi.h>
#include <HTTPClient.h> 
#include <ArduinoJson.h>
#include <SimpleDHT.h> 
#include <Wire.h>
#include <BH1750.h>
#include <base64.h>
#include <Adafruit_ADS1015.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <iostream>
#include <string>
#include <Preferences.h>

// Criando uma instância da biblioteca preferences (para salvar em memória persistente)
Preferences preferences;

// Configurações do bluetooth
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
String ble_enviado;
String ble_recebido;

// Wifi
String ssid = "";
String password = "";

// Endereço root do servidor
String server_name = "http://gabrielsalom.pythonanywhere.com";

// Usuário e senha para comunicação servidor
String authUsername = "beleaf_green";
String authPassword = "beleaf_teste";

// Configuração Json
char jsonOutput[128];           

// Configuração Bh1750
BH1750 lightMeter;
float lux;

// Configuração Dht22
#define DHTPIN 4
SimpleDHT22 dht;
float temperature, humidity, t, h;

// Pinagem correspondente da fita led
const int ledPin = 23;  // 23 corresponds to GPIO16

// Parametros para acionamento do PWM
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;
int dutyCycle = 0;

// Configuração TDS
const int TdsSensorPin = 33;
int c_bits = 0; 
int conductivity;
int leitura = 0;

// Configuração PH
Adafruit_ADS1115 ads;
float ph;

// Configuração relé para acionamento da bomba
const int relePin =  32;
int pump_status = 0;

// Configurações dos parâmetros iniciais
int lux_max = 50;
int lux_min = 40;
int time_on = 10;
int time_off = 10;
int automatic_light = 0;
int light_intensity = 0;

// Variaveis para controle preciso do tempo
unsigned long StartTime = millis();
unsigned long LuxStartTime = millis();
unsigned long PumpStartTime = millis();

// Classes para comunicação bluetooth
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };
 
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      // Notificando o aparelho que a informação foi recebida
      ble_enviado = "Recebido!";
      pCharacteristic->setValue(ble_enviado.c_str());

      // Armazenando SSID e Senha recebidos
      if (rxValue.length() > 0) {
        ble_recebido = "";
        for (int i = 0; i < rxValue.length(); i++)
          {
            ble_recebido = ble_recebido + rxValue[i];
          }
        }
     
        if(ble_recebido.startsWith("i:"))
        {
          ble_recebido.remove(0, 2);
          ssid = ble_recebido;
          preferences.remove("Kssid");
          preferences.putString("Kssid", ssid);
        }
        if(ble_recebido.startsWith("p:"))
        {
          ble_recebido.remove(0, 2);
          password = ble_recebido;
          preferences.remove("Kpass");
          preferences.putString("Kpass", password);
        }
    }
};
void setup() {

  Serial.begin(9600);

  // Inicializando "wifi_login" no modo "Read-Write"
  preferences.begin("wifi_login", false);

  // Configuração BLE
  BLEDevice::init("Beleaf");
 
  // Configura o dispositivo como Servidor BLE
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
 
  // Cria o servico UART
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // cria uma característica BLE para recebimento dos dados
  pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ   |
                                         BLECharacteristic::PROPERTY_WRITE  |
                                         BLECharacteristic::PROPERTY_NOTIFY 
                                       );
                                       
  pCharacteristic->addDescriptor(new BLE2902());
 
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("Iniciado.");
  // Inicia o serviço
  pService->start();
 
  // Inicia a descoberta do ESP32
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
  
  pServer->getAdvertising()->start();
  Serial.println("Esperando um cliente se conectar...");
  
  // Rele
  pinMode(relePin, OUTPUT);
  
  // Start na biblioteca I2C e sensor de luminosidade
  Wire.begin();
  lightMeter.begin();
  
  // Configuração do PWM para controle do LED
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(ledPin, ledChannel);

  // TDS 
  pinMode(TdsSensorPin,INPUT);

  // PH
  ads.setGain(GAIN_TWOTHIRDS);
  ads.begin();

  ssid = preferences.getString("Kssid", ssid);
  password = preferences.getString("Kpass", password);       
  Serial.println("ssid:" + String(ssid) + "\n" + "password:" + String(password));
  if((ssid != NULL) && (password != NULL))
  {
    Serial.println("Tentando conectar");
    connect_wifi(ssid, password);
  }
  else
  {
    Serial.println("SSID ou senha não informados");
  }
}

// A função a seguir recebe como parametros SSID e password e tenta se conectar na rede wifi
void connect_wifi(String ssid, String password)
{
    WiFi.begin(ssid.c_str(), password.c_str());
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) 
    {
      delay(1000);
      ble_enviado = "conectando. " + String(i) + "s";
      Serial.println(ble_enviado);
      pCharacteristic->setValue(ble_enviado.c_str()); // Return status
      pCharacteristic->notify();
      i++;
      if(i > 20)
      {
        ble_enviado = "tempo esgotado";
        pCharacteristic->setValue(ble_enviado.c_str()); // Return status
        pCharacteristic->notify();
        break;
      }
    }
    if(WiFi.status() == WL_CONNECTED)
    {
      Serial.println("conectado");
      ble_enviado = "conectado";
      pCharacteristic->setValue(ble_enviado.c_str()); // Return status
      pCharacteristic->notify();
    }
    else
    {
      ble_enviado = "rede/senha incorretos";
      pCharacteristic->setValue(ble_enviado.c_str()); // Return status
      pCharacteristic->notify();
    }
}

 // Função para enviar dados dos sensores para o servidor
void sending_data(float lux, float temperature, float humidity, float ph, float conductivity){
   // Objeto de comunicação HTTP
   HTTPClient http; 
   
   // variavel contendo autenticação
   String auth = base64::encode(authUsername + ":" + authPassword);

   // Rota no servidor para os dados a serem enviados via POST
   http.begin(server_name + "/chart_data");  
   // Criando cabeçalho com autentificação
   http.addHeader("Content-Type", "application/json", "Authorization", "Basic " + auth); 

   // Atribuíndo os valores ao pacote Json
   const size_t CAPACITY = JSON_OBJECT_SIZE(5);
   StaticJsonDocument<CAPACITY> doc;
   JsonObject object = doc.to<JsonObject>();
   object["light"] = lux;
   object["temperature"] = temperature;
   object["humidity"] = humidity;
   object["ph"] = ph;
   object["conductivity"] = conductivity;
   serializeJson(doc, jsonOutput);

   // Enviando os dados
   int httpResponseCode = http.POST(String(jsonOutput));
   // Caso tenha recibido a resposta
   if(httpResponseCode>0){
    Serial.println(httpResponseCode); 
   }
   // Caso não seja recebido uma resposta
   else
   {
    Serial.println(httpResponseCode);
    Serial.println("Erro ao enviar o POST");
   }
   // Liberando os recursos
   http.end();
}

// Função para checar a necessidade de alterar parametros de controle da bomba ou luminosidade
void recieving_data(int *lux_max, int *lux_min, int *time_on, int *time_off, int *light_intensity, int *automatic_light){
   // Objeto de comunicação HTTP
   HTTPClient http; 
   
   // variavel contendo autenticação
   String auth = base64::encode(authUsername + ":" + authPassword);
   // Rota no servidor para os dados a serem recebidos via GET
   http.begin(server_name + "/config_elements");  //Specify destination for HTTP request
   http.addHeader("Authorization", "Basic " + auth); 
   // Enviando requisição HTTP GET
   int httpResponseCode = http.GET();
   // Caso tenha recibido a resposta
      if (httpResponseCode>0) {
        Serial.println(httpResponseCode);
        String payload = http.getString();
        //Serial.println(payload);
        char json[500];
        payload.replace(" ", "");
        payload.replace("\n", "");
        payload.trim();
        payload.toCharArray(json, 500);

        StaticJsonDocument<200> doc;
        deserializeJson(doc, json);
        
        *lux_max = doc["lux_max"];
        *lux_min = doc["lux_min"];
        *time_on = doc["time_on"];
        *time_off = doc["time_off"];
        *light_intensity = doc["light_intensity"];
        *automatic_light = doc["automatic_light"];
        
      }
    // Caso não seja recebido uma resposta
      else {
        Serial.println(httpResponseCode);
        Serial.println("Erro ao enviar o GET");
      }
      // Liberando os recursos
      http.end();
}
void readdht()
{
 
  //Coloca o valor lido da temperatura em t e da umidade em h
  int status = dht.read2(DHTPIN, &t, &h, NULL);
 
  //Se a leitura foi bem sucedida
  if (status == SimpleDHTErrSuccess) {
    //Os valores foram lidos corretamente, então é seguro colocar nas variáveis
    temperature = t;
    humidity = h;
  }
  else{
    temperature = NULL;
    humidity = NULL;
  }
}

void readbh1750()
{
  lux = lightMeter.readLightLevel();
  if (isnan(lux) || lux < 0) {
      //Serial.println("Failed to read from lux sensor!");
      lux = NULL;
  }
}

void readph()
{
  int16_t adc0, adc1, adc2, adc3;
  ph = ads.readADC_SingleEnded(2);
  ph = 100*ph/65535; // conversão de bits
  if (isnan(ph))
    {
        //Serial.println(F("Failed to read from ph sensor!"));
        ph = NULL;
    }
}

void readcondutivity()
{
  c_bits = analogRead(TdsSensorPin);
  conductivity = c_bits*100/4095;
  if (isnan(conductivity)) 
  {
      //Serial.println(F("Failed to read from lux sensor!"));
      conductivity = NULL;
  }
}

void loop() 
{  

    // Verificação de quanto tempo decorreu
    unsigned long CurrentTime = millis();
    unsigned long LuxElapsedTime = (CurrentTime - LuxStartTime); // Tempo que se passou em segundos para leitura do sensor de luz
    unsigned long ElapsedTime = (CurrentTime - StartTime)/1000; // Tempo que se passou em segundos para envio/recebimento de informações ao servidor
    unsigned long PumpElapsedTime = (CurrentTime - PumpStartTime)/1000; // Tempo que se passou em segundos para controle da bomba

    if (LuxElapsedTime > 500)
    {
        // Leitura de luminosidade
        readbh1750();

        // Verificação se o controle de luminosidade deve ser feito automatico
        if(automatic_light == 1)
        { 
          // Verificação se a leitura do sensor de luminosidade está adequada
            if(lux != NULL)
            {
              if(lux < lux_min)
              {
                if(dutyCycle < 180)
                  {
                    dutyCycle++;
                    ledcWrite(ledChannel, dutyCycle);
                  }
              }
              else if(lux > lux_max)
              {
                  if(dutyCycle > 0)
                  {
                    dutyCycle--;
                    ledcWrite(ledChannel, dutyCycle);
                  }
              }
            }
            else
            {
              dutyCycle = 0;
              ledcWrite(ledChannel, dutyCycle);
            }
            LuxStartTime = millis();
        }
        if(automatic_light == 0)
        {
          if(dutyCycle != light_intensity)
          {
            dutyCycle = light_intensity;
            ledcWrite(ledChannel, dutyCycle);
          }
        }
      Serial.println("automatic_light :" + String(automatic_light) + "\n" + "dutyCycle:" + String(dutyCycle));
    }

    // Realiza uma amostragem dos sensores e envia/recebe dados do servidor a cada 10 segundos
    if (ElapsedTime > 10)
    {
     
      // Leitura do DHT22
      readdht();
      
      // Leitura do sensor de PH
      readph();
      
      // Leitura do sensor de condutividade
      readcondutivity();

      StartTime = millis(); // Reset no contador de tempo
      Serial.println("Lux :" + String(lux) + "\n" + "Temperature:" + String(temperature) + "\n" + "Humidity:" + String(humidity) + "\n" + "ph:" + String(ph) + "\n" + "conductivity:" + String(conductivity) + "\n");
      //Serial.println("Lux max:" + String(lux_max) + "\n" + "Lux min:" + String(lux_min) + "\n" + "time_on:" + String(time_on) + "\n" + "time_off:" + String(time_on) + "\n");
      if (WiFi.status()== WL_CONNECTED)
      {
          ble_enviado = "wifi conectado";
          pCharacteristic->setValue(ble_enviado.c_str()); // Return status
          pCharacteristic->notify();
          recieving_data(&lux_max, &lux_min, &time_on, &time_off, &light_intensity, &automatic_light); // Verifica se existe alguma atualização nos parametros de controle
          sending_data(lux, temperature, humidity, ph, conductivity); // envia os dados para o servidor
      }
      if (WiFi.status()!= WL_CONNECTED)
      {
        Serial.println("Erro na conexão wifi"); 
        ble_enviado = "desconectado";
        pCharacteristic->setValue(ble_enviado.c_str()); // Return status
        pCharacteristic->notify();

        ssid = preferences.getString("Kssid", ssid);
        password = preferences.getString("Kpass", password);       
        Serial.println("ssid:" + String(ssid) + "\n" + "password:" + String(password));
        if((ssid != NULL) && (password != NULL))
        {
          Serial.println("Tentando conectar");
          connect_wifi(ssid, password);
        }
        else
        {
          Serial.println("SSID ou senha não informados");
        }
      }
    }

    // Verifica o status da bomba e liga e desliga ela de acordo com os parametros atualizados
    if (pump_status == 0){
      if (PumpElapsedTime >= time_off){
        Serial.println("Bomba ligada :" + String(PumpElapsedTime) + "\n");
        PumpStartTime = millis();
        PumpElapsedTime = (CurrentTime - PumpStartTime)/1000;
        digitalWrite(relePin, HIGH);
        pump_status = 1;
      }
    }
    if (pump_status == 1){
      if (PumpElapsedTime >= time_on){
        Serial.println("Bomba desligada :" + String(PumpElapsedTime) + "\n");
        PumpStartTime = millis();
        PumpElapsedTime = (CurrentTime - PumpStartTime)/1000;
        digitalWrite(relePin, LOW);
        pump_status = 0;
      }
    }
}
