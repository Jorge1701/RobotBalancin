#include <Wire.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>

/* Conexion WebSocket */

#define PULSO_INTERVALO 25000

uint64_t pulsoTiempo = 0;
bool estaConectado = false;

char* ssid = "TamoBunker";
char* pass = "ladesiempre";

char* host = "192.168.1.5";
int puerto = 3000;

ESP8266WiFiMulti wf;
WebSocketsClient ws;

/* Giroscopio */

const int MPU = 0x68;

const float A_R = 16384.0;
const float G_R = 131.0;
const float RAD_A_DEG = 57.295779;

const int en1 = 2;
const int en2 = 0;

const int in1 = 15;
const int in2 = 13;
const int in3 = 12;
const int in4 = 14;

float accX, accY;
float gyrX, gyrY;
float x, y;
float anterior;

int dir = 0;
int tiempoGiro = 0;

String getDato ( uint8_t * contenido ) {
  String res = "";
  int i = 0;

  while ( ( ( char ) contenido[i] ) != '"' )
    i++;
  i++;

  while ( 1 ) {
    if ( ( ( char ) contenido[i] ) == '"' )
      break;
    
    res += ( char ) contenido[i];
    i++;
  }
  
  return res;
}

String getValor ( uint8_t * contenido ) {
  String res = "";
  int i = 0;
  int c = 0;

  while ( c != 3 ) {
    if ( ( ( char ) contenido[i] ) == '"' )
      c++;
    
    i++;
  }
  
  while ( 1 ) {
    if ( ( ( char ) contenido[i] ) == '"' )
      break;
    
    res += ( char ) contenido[i];
    i++;
  }
  
  return res;
}

void wsEvento ( WStype_t tipo, uint8_t * contenido, size_t tam ) {
  switch ( tipo ) {
    case WStype_DISCONNECTED:
      Serial.println( "WebSocket NO conectado!" );
      estaConectado = false;
      break;
    case WStype_CONNECTED:
      Serial.printf( "WebSocket connectado a %s\n", contenido );
      estaConectado = true;
      ws.sendTXT( "5" );
      break;
    case WStype_TEXT:
      if ( contenido[0] == '4' && contenido[1] == '2' ) {
        String dato = getDato( contenido );
        String valor = getValor( contenido );
        
        if ( dato == "GIRO_IZQ" ) {
          dir = 1;
          tiempoGiro = valor.toInt();
          Serial.print( "IZQ" );
          Serial.println( tiempoGiro );
        } else {
          dir = 2;
          tiempoGiro = valor.toInt();
          Serial.print( "DER" );
          Serial.println( tiempoGiro );
        }
      }
      
      break;
  }
}

void emit ( String data, String valor ) {
  ws.sendTXT( "42[\"" + data + "\",\"" + valor + "\"]" );
}

void setup() {
  Serial.begin( 115200 );

  for ( uint8_t t = 3; t > 0; t--) {
    Serial.printf( "Conectando en %d...\n", t );
    delay( 1000 );
  }

  wf.addAP( ssid, pass );

  while ( wf.run() != WL_CONNECTED )
    delay( 100 );

  Serial.printf( "Conectado a %s con la IP: ", ssid );
  Serial.println( WiFi.localIP() );

  ws.beginSocketIO( host, puerto );
  ws.onEvent( wsEvento );

  /* -------- */
  
  Wire.begin( 4, 5 );

  Wire.beginTransmission( MPU );
  Wire.write( 0x6B );
  Wire.write( 0 );
  Wire.endTransmission( true );
  
  pinMode( en1, OUTPUT );
  pinMode( en2, OUTPUT );

  pinMode( in1, OUTPUT );
  pinMode( in2, OUTPUT );
  pinMode( in3, OUTPUT );
  pinMode( in4, OUTPUT );
}

void loop() {
  ws.loop();

  if ( estaConectado ) {
    uint64_t ahora = millis();

    if ( ahora - pulsoTiempo > PULSO_INTERVALO ) {
      pulsoTiempo = ahora;
      ws.sendTXT( "2" );
    }
  }
  
  Wire.beginTransmission( MPU );
  Wire.write( 0x3B );
  Wire.endTransmission( false );
  Wire.requestFrom( MPU, 6, true );

  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();

  accY = atan( -1 * ( ax / A_R ) / sqrt( pow( ( ay / A_R ), 2 ) + pow( ( az / A_R ), 2 ) ) ) * RAD_A_DEG;
  accX = atan(      ( ay / A_R ) / sqrt( pow( ( ax / A_R ), 2 ) + pow( ( az / A_R ), 2 ) ) ) * RAD_A_DEG;

  Wire.beginTransmission( MPU );
  Wire.write( 0x43 );
  Wire.endTransmission( false );
  Wire.requestFrom( MPU, 4, true );
  
  int16_t gx = Wire.read() << 8 | Wire.read();
  int16_t gy = Wire.read() << 8 | Wire.read();

  gyrX = gx / G_R;
  gyrY = gy / G_R;

  float ahora = millis();
  float t = ( ahora - anterior ) / 1000;
  
  x = 0.98 * ( x + gyrX * t ) + 0.02 * accX;
  y = 0.98 * ( y + gyrY * t ) + 0.02 * accY;

  if ( dir == 0 )
    mover( abs( x ) <= 5 ? 0 : x );
  else {
    if ( dir == 1 ) {
      digitalWrite( in1, HIGH );
      digitalWrite( in2, LOW );
      digitalWrite( in3, HIGH );
      digitalWrite( in4, LOW );
    } else {
      digitalWrite( in1, LOW );
      digitalWrite( in2, HIGH );
      digitalWrite( in3, LOW );
      digitalWrite( in4, HIGH );
    }
    
    analogWrite( en1, 1023 );
    analogWrite( en2, 1023 );
    
    tiempoGiro -= ( ahora - anterior );
    
    if ( tiempoGiro <= 0 )
      dir = 0;
  }
  
  anterior = ahora;
}

void mover( float inclinacion ) {
  int tiempo = ( int ) ( ( float ) abs( inclinacion ) * ( float ) 23 );

  if ( tiempo >= 1023 )
    tiempo = 1023;

  if ( inclinacion == 0 ) {
    digitalWrite( in1, LOW );
    digitalWrite( in4, LOW );
    digitalWrite( in2, LOW );
    digitalWrite( in3, LOW );
  } else if ( inclinacion < 0 ) {
    digitalWrite( in1, LOW );
    digitalWrite( in4, LOW );
    digitalWrite( in2, HIGH );
    digitalWrite( in3, HIGH );
  } else {
    digitalWrite( in2, LOW );
    digitalWrite( in3, LOW );
    digitalWrite( in1, HIGH );
    digitalWrite( in4, HIGH );
  }

 
  analogWrite( en1, tiempo );
  analogWrite( en2, tiempo );
}
