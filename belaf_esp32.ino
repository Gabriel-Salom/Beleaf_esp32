#include <WiFi.h>
#include <HTTPClient.h> 
#include <ArduinoJson.h>
#include "DHT.h"
#include <Wire.h>
#include <BH1750.h>
#include <base64.h>
#include <Adafruit_ADS1015.h>

#define DHTPIN 4
#define DHTTYPE DHT11

// SSID e senha wifi
const char* ssid = "Coelho";
const char* password =  "01051430";

// Nome do servidor
String server_name = "http://gabrielsalom.pythonanywhere.com";

// Usuário e senha para comunicação servidor
String authUsername = "beleaf_green";
String authPassword = "beleaf_teste";

char jsonOutput[128];

// Iniciando sensor DHT
DHT dht(DHTPIN, DHTTYPE);                

// Iniciando sensor de luminosidade
BH1750 lightMeter;

// Pinagem correspondente da fita led
const int ledPin = 23;  // 23 corresponds to GPIO16

// Parametros para acionamento do PWM
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;
int dutyCycle = 0;

// Variaveis do sensor de TDS
const int TdsSensorPin = 13;
int c = 0; 
int leitura = 0;

// Sensor PH
 Adafruit_ADS1115 ads;

// Relé para acionamento da bomba
const int relePin =  32;

// Configurações de parametros iniciais
int lux_max = 50;
int lux_min = 40;
int time_on = 10;
int time_off = 10;

// Variaveis para controle preciso do tempo
unsigned long StartTime = millis();
unsigned long PumpStartTime = millis();
int pump_status = 0;

void setup() {

  Serial.begin(9600);

  // Rele
  pinMode(relePin, OUTPUT);
  
  // Start na biblioteca I2C e sensor de luminosidade
  Wire.begin();
  lightMeter.begin();
  
  // Configuração do PWM para controle do LED
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(ledPin, ledChannel);

  // DHT 11 
  //pinMode(DHTPIN, INPUT);
  dht.begin(); 

  // TDS 
  pinMode(TdsSensorPin,INPUT);

  // PH
  ads.setGain(GAIN_TWOTHIRDS);
  ads.begin();

  //Wifi Start
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
}

 // Função para enviar dados dos sensores para o servidor
void sending_data(float lux, float Temperature, float Humidity, float ph, float conductivity){
 if(WiFi.status()== WL_CONNECTED)  //Checagem do status wifi
  {
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
   object["temperature"] = Temperature;
   object["humidity"] = Humidity;
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
   else{
    Serial.println(httpResponseCode);
    Serial.println("Erro ao enviar o POST");
   }
   // Liberando os recursos
   http.end();
 }else{
    Serial.println("Error in WiFi connection");   
 }
}

// Função para checar a necessidade de alterar parametros de controle da bomba ou luminosidade
void recieving_data(int *lux_max, int *lux_min, int *time_on, int *time_off){
if(WiFi.status()== WL_CONNECTED)   //Check WiFi connection status
  {
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
      }
    // Caso não seja recebido uma resposta
      else {
        Serial.println(httpResponseCode);
        Serial.println("Erro ao enviar o GET");
      }
      // Liberando os recursos
      http.end();
  }
  else{
  
      Serial.println("Error in WiFi connection");   
  }
}

void loop() 
{  

// Leitura do DHT11
float Temperature = dht.readHumidity();
float Humidity = dht.readTemperature();
if (isnan(Temperature) || isnan(Humidity)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    Temperature = NULL;
    Humidity = NULL;
}

// Leitura do sensor de PH
int16_t adc0, adc1, adc2, adc3;
float ph = ads.readADC_SingleEnded(2);
if (isnan(ph)) {
    Serial.println(F("Failed to read from ph sensor!"));
    ph = NULL;
}

// Leitura de luminosidade
float lux = lightMeter.readLightLevel();
if (isnan(lux) || lux < 0) {
    Serial.println(F("Failed to read from lux sensor!"));
    lux = NULL;
}

// Leitura do sensor de condutividade
c = analogRead(TdsSensorPin);
float conductivity = c*100/4095;
if (isnan(conductivity)) {
    Serial.println(F("Failed to read from lux sensor!"));
    conductivity = NULL;
}
  /*
// Teste
float Temperature = random (100);
float Humidity = random (100);
float ph = random (100);
float lux = random (100);
float conductivity  = random (100);
*/
// Verificação se a leitura do sensor de luminosidade está adequada
  if(lux != NULL)
  {
    if(lux < lux_min)
    {
      if(dutyCycle < 255)
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
  else{
    dutyCycle = 0;
  }
    // Verificação de quanto tempo decorreu
    unsigned long CurrentTime = millis();
    unsigned long ElapsedTime = (CurrentTime - StartTime)/1000; // Tempo que se passou em segundos para envio/recebimento de informações ao servidor
    int PumpElapsedTime = (CurrentTime - PumpStartTime)/1000; // Tempo que se passou em segundos para controle da bomba

    // Envia/recebe dados do servidor a cada 10 segundos
    if (ElapsedTime > 10){
      StartTime = millis(); // Reset no contador de tempo
      recieving_data(&lux_max, &lux_min, &time_on, &time_off); // Verifica se existe alguma atualização nos parametros de controle
      sending_data(lux, Temperature, Humidity, ph, conductivity); // envia os dados para o servidor
      Serial.println("Lux max:" + String(lux_max) + "\n" + "Lux min:" + String(lux_min) + "\n" + "time_on:" + String(time_on) + "\n" + "time_off:" + String(time_on) + "\n");
    }

    // Verifica o status da bomba e liga e desliga ela de acordo com os parametros atualizados
    if (pump_status == 0){
      if (PumpElapsedTime > time_off){
        PumpStartTime = millis();
        PumpElapsedTime = (CurrentTime - PumpStartTime)/1000;
        digitalWrite(relePin, HIGH);
        pump_status = 1;
        Serial.println("Bomba ligada");
      }
    }
    if (pump_status == 1){
      if (PumpElapsedTime > time_on){
        PumpStartTime = millis();
        PumpElapsedTime = (CurrentTime - PumpStartTime)/1000;
        digitalWrite(relePin, LOW);
        pump_status = 0;
        Serial.println("Bomba desligada");
      }
    }
}
