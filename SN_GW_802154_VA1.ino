/*
MQTT-SN to MQTT Gateway implementation based on MRF24J40 ESP12 NodeMCU with Arduino code
This is an MQTT-SN to MQTT Gateway implementation based on MRF24J40 and NodeMCU with Arduino code
The implementation is based ot MQTT-SN Specification Version 1.2.
An ESP12 NodeMCU module is used for both executing the Gateway functions and connecting to the Internet over WiFi.

Version Alpha 1 (VA1)

This is a very early implementation. Part of the whole message list according specs is supported so far.
Topics should be predefined on the client and gateway sides and are fixed to 2 positions alpha-numeric string.

Messages supported:

CONNECT
CONNACK
PINGREQ
PINGRESP
PUBLISH (both directions)
SUBSCRIBE
SUBACK

*/

#include <SPI.h>
#include <mrf24j.h>   // *** Please use the modified library found within the same repo on GitHub ***
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//#include <String.h>

//WiFi settings
const char* ssid = "Enter your WiFi SSID here";
const char* password = "Enter your WiFi Password here";

//MQTT Settings
const char* mqtt_server = "Enter your MQTT Server IP address here";
#define MQTT_CLIENT "SN_Gateway"

char conn_addr[5];
const char* conn_msg = "CONN";
const char* tout_msg = "TOUT";

    const int pin_reset = 5; // GPIO 5, 6 = default
    const int pin_cs = 15; // GPIO 15, 10 = default CS pin on ATmega8/168/328
    const int pin_interrupt = 4; // GPIO 4, 2 = default interrupt pin on ATmega8/168/328

Mrf24j mrf(pin_reset, pin_cs, pin_interrupt);
WiFiClient espClient;
PubSubClient mqttClient(mqtt_server, 1883, espClient);



long current_time;
long last_time;
long tx_interval = 1000;
long last_update;
long update_interval = 10000;

boolean message_received = false;
boolean mqtt_msg_received = false;
char rx_buffer[127];
uint8_t tx_buffer[127];
uint8_t mqtt_rx_topic[3];
uint8_t mqtt_rx_payload[16];  // TBD
int rx_len;

typedef struct
{
  uint8_t Length;
  uint8_t MsgType;
  uint8_t Var[127];
} Message;

Message Msg;
Message TxMsg;

typedef struct
{
  uint8_t Flags;
  uint8_t ProtocolID;
  uint8_t Duration[2];
  char ClientID[16];
} ConnectMsg;

ConnectMsg ConnMsg;

typedef struct
{
  char ClientID[16];
} PingMsg;

PingMsg PingreqMsg;

typedef struct
{
  uint16_t address;
  boolean client_connected;
  long timer;
  long duration;
} Clients;

Clients clients[255];

typedef struct
{
  uint8_t  address[2];
  uint8_t  topic_id[2];
} Subscriptions;

Subscriptions sub_table[1024];

typedef struct
{
  uint8_t Flags;
  uint8_t topic_id[2];
  uint8_t msg_id[2];
  uint8_t data[15];
} PublishMsg;

PublishMsg pubMsg;

typedef struct
{
  uint8_t Flags;
  uint8_t msg_id[2];
  uint8_t topic_id[2];
} SubscribeMsg;

SubscribeMsg subMsg;

uint8_t clientsn[4] = "N/A";

uint8_t clients_total;
uint16_t client_address;
uint16_t client_duration;

// MQTT-SN Commands
#define CONNECT     0x04
#define CONNACK     0x05
#define PINGREQ     0x16
#define PINGRESP    0x17
#define PUBLISH     0x0C
#define PUBACK      0x0D
#define SUBSCRIBE   0x12
#define SUBACK      0x13
#define UNSUBSCRIBE 0x14
#define UNSUBACK    0x15
#define DISCONNECT  0x18

// States
#define CONNECTED     0x01
#define NOTCONNECTED  0x00

void callback(char* topic, byte* payload, unsigned int length);

void setup() {
  WiFi.mode(WIFI_STA);
  
  Serial.begin(115200);
  
  Serial.println("MQTT-SN Gateway implementation VA1");
  Serial.println("==================================");
  
  mrf.reset();
  mrf.init();

  mrf.set_pan(0xcfce); // pan ID = 0xCFCE (Cat FaCE)

  mrf.address16_write(0x8266); // own address = 0x8266

  mrf.rx_flush();

  // uncomment if you want to receive any packet on this channel
  //mrf.set_promiscuous(true);
  
  // uncomment if you want to enable PA/LNA external control
  //mrf.set_palna(true);
  
  // uncomment if you want to buffer all PHY Payload
  //mrf.set_bufferPHY(true);

  attachInterrupt(4, interrupt_routine, CHANGE); // interrupt connected to GPIO4
  
  interrupts();

  mqttClient.setCallback(callback);

  current_time = millis();

  last_time = current_time + update_interval + 1;


  Serial.println("Module MRF24J40MA Init done.");

  Serial.print("Connecting WiFi Network");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Connecting MQTT Server");
  while (!mqttClient.connect(MQTT_CLIENT))
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("MQTT Connected");
  
  pinMode(LED_BUILTIN, OUTPUT);

  clients[0].address = 0xFFFF;  // record 0 = 0xFFFF
  clients[0].client_connected = false;      // no clients connected
  clients_total = 0;

  sub_table[0].address[0]=0xFF;
  sub_table[0].address[1]=0xFF;

}

void interrupt_routine() {
    mrf.interrupt_handler(); // mrf24 object interrupt routine
}

void loop()
{
    uint8_t i, j, move_counter;
    mrf.check_flags(&handle_rx, &handle_tx);

    if(message_received)
    {
//      mrf.send16(0x1414,rx_buffer,rx_len);  // Forward message to 0x1414 for test purposes only

      Msg.Length = rx_buffer[0];  // Fill in the message fields
      Msg.MsgType = rx_buffer[1];
      for(i=2;i<Msg.Length;i++)
      {
        Msg.Var[i-2] = rx_buffer[i];
      }

      switch (Msg.MsgType)        // Check message type
      {
        case(CONNECT):
        {
          ConnMsg.Flags = rx_buffer[2];
          ConnMsg.ProtocolID = rx_buffer[3];
          ConnMsg.Duration[0] = rx_buffer[4];
          ConnMsg.Duration[1] = rx_buffer[5];
          client_duration = 256*rx_buffer[4] + rx_buffer[5];
//          Serial.print("CONNECT from 0x");
//          Serial.println(client_address,HEX);
          for(i=6;i<Msg.Length-2;i++)
          {
//              Serial.println("x");
//            ConnMsg.ClientID[i-6] = rx_buffer[i];
//            Serial.print((char)ConnMsg.ClientID[i-6]);
          }
          client_address = 256*rx_buffer[i] + rx_buffer[i+1];
          Serial.print("CONNECT from 0x");
          Serial.println(client_address,HEX);
//          Serial.println("\nClient address: ");
//          Serial.println(Msg.Length,HEX);

//          ConnMsg.ClientID[i-6]=0x00;   // line edn of client id
          
          i=0;
          do
          {
            if(clients[i].address == client_address)
            {
            if(clients[i].client_connected)
              {
                Serial.print("Client is already connected: 0x");
                Serial.println(client_address,HEX);
                break;
              }
//              else
//              {
//                Serial.print("CONNACK to: 0x");
//                Serial.println(client_address,HEX);
//                tx_buffer[0] = 0x03;  // Fill in the message fields
//                tx_buffer[1] = CONNACK;            
//                tx_buffer[2] = 0x00;                    // TODO - Change with defined constant
//                mrf.send16((word)client_address,(char*)tx_buffer, tx_buffer[0]);  // Send CONNACK back
//                clients[i].client_connected = true;
//                clients[i].timer = 10;
//              }
            }
            if(clients[i].address == 0xFFFF)
            {
              clients[i].address = client_address;
              clients[i].duration = client_duration;
              clients[i].timer = clients[i].duration;
              clients[i+1].address = 0xFFFF;
              Serial.print("New connection from: 0x");
              Serial.println(client_address,HEX);
              Serial.print("Client duration: 0x");
              Serial.println(clients[i].duration,HEX);

              String(client_address,HEX).toCharArray(conn_addr,5);
              mqttClient.publish(conn_addr,conn_msg); // Send message to MQTT broker for the new client connected

              Serial.print("CONNACK to 0x");
              Serial.println(client_address,HEX);
              tx_buffer[0] = 0x03;  // Fill in the message fields
              tx_buffer[1] = CONNACK;            
              tx_buffer[2] = 0x00;                    // TODO - Change with defined constant
              mrf.send16((word)client_address,(char*)tx_buffer, tx_buffer[0]);  // Send CONNACK back
//              mrf.send16((word)client_address,(char*)tx_buffer, tx_buffer[0]);
              clients[i].client_connected = true;
              break;
            }
            i++;
          } while(i<0xFF);
        } break;
        
        case(PINGREQ):
        {

          for(i=2;i<Msg.Length-2;i++);
//          {
//            PingreqMsg.ClientID[i-2] = rx_buffer[i];
//            Serial.print((char)PingreqMsg.ClientID[i-2]);
//          }

          client_address = 256*rx_buffer[i] + rx_buffer[i+1];
          
          Serial.print("PINGREQ from 0x");
          Serial.println(client_address,HEX);

          for(i=0;(clients[i].address!=client_address)&&(clients[i].address!=0xFFFF);i++);

          if(clients[i].address==0xFFFF)
          {
            Serial.print("\nClient is not connected: 0x");
            Serial.println(client_address,HEX);

          }
          else if(clients[i].address == client_address)
          {
            if(clients[i].client_connected)
            {
//            Serial.print("\nClient connected: 0x");
//            Serial.println(client_address,HEX);
//            PingreqMsg.ClientID[(i-2)+1]=0x00;
//            client_address = 256*rx_buffer[i] + rx_buffer[i+1];
//            Serial.println();
            tx_buffer[0] = 0x02;  // Fill in the message fields
            tx_buffer[1] = PINGRESP;
            mrf.send16((word)client_address,(char*)tx_buffer,tx_buffer[0]);
            clients[i].timer = clients[i].duration;
            clients[i].client_connected = true;
            Serial.print("PINGRESP to 0x");
            Serial.println(client_address,HEX);
            }
            else
            {
              Serial.println("Client is not connected");
            }

          }
        } break;

        case(PUBLISH):
        {
          uint8_t dest[3];
          
          Serial.print("PUBLISH received (hex): ");
          pubMsg.Flags = rx_buffer[2];
          pubMsg.topic_id[0] = rx_buffer[3];
          pubMsg.topic_id[1] = rx_buffer[4];
          pubMsg.msg_id[0] = rx_buffer[5];
          pubMsg.msg_id[1] = rx_buffer[6];

          for(i=7;i<Msg.Length;i++)
          {
              pubMsg.data[i-7] = rx_buffer[i];
              Serial.print(pubMsg.data[i-7],HEX);
              Serial.print(" ");
          }
          pubMsg.data[i]=0x00;
//          while(i<23)
//          {
////            Serial.println("Clearing buffer...");
//            
//            pubMsg.data[i-7]=0x00;  // Clean up the rest of the buffer
//            i++;
//           }
          Serial.println();

          dest[0] = pubMsg.topic_id[0];
          dest[1] = pubMsg.topic_id[1];
          dest[3] = 0x00;
          Serial.print("Destination: ");
          Serial.println((char *)dest);
          
          mqttClient.publish((char *)dest,(char *)pubMsg.data);
        } break;

        case(SUBSCRIBE):
        {
          uint8_t topic[3];
          
          Serial.println("SUBSCRIBE received");
          subMsg.Flags = rx_buffer[2];
          subMsg.msg_id[0] = rx_buffer[3];
          subMsg.msg_id[1] = rx_buffer[4];
          subMsg.topic_id[0] = rx_buffer[5];
          subMsg.topic_id[1] = rx_buffer[6];
          
          topic[0] = subMsg.topic_id[0];
          topic[1] = subMsg.topic_id[1];
          topic[2] = 0x00;
          
          i=0;
          boolean already_subscribed = false;
          
          while((sub_table[i].address[0] != 0xFF) && (sub_table[i].address[1] != 0xFF))
          {
            if(sub_table[i].address[0]==subMsg.msg_id[0])
              if(sub_table[i].address[1]==subMsg.msg_id[1])
                if(sub_table[i].topic_id[0]==subMsg.topic_id[0])
                  if(sub_table[i].topic_id[1]==subMsg.topic_id[1])
                    {
                      Serial.println("Client is already subscribed for that topic.");
                      already_subscribed = true;
                    }
            i++;
           }
           Serial.print("Topics registered so far: ");
           Serial.println(i);
           if(!already_subscribed)
           {
             
               Serial.print("Subscribing topic: ");
               Serial.println((char *)topic);
              
               mqttClient.subscribe((char *)topic);
                       
               sub_table[i].address[0]=subMsg.msg_id[0];
               sub_table[i].address[1]=subMsg.msg_id[1];
               sub_table[i].topic_id[0]=subMsg.topic_id[0];
               sub_table[i].topic_id[1]=subMsg.topic_id[1];
               sub_table[i+1].address[0]=0xFF;
               sub_table[i+1].address[1]=0xFF;            
           }
           
           tx_buffer[0] = 0x08;             // Fill in the message fields
           tx_buffer[1] = SUBACK;
           tx_buffer[2] = 0x00;             // Flags;
           tx_buffer[3] = subMsg.topic_id[0];
           tx_buffer[4] = subMsg.topic_id[1];
           tx_buffer[5] = subMsg.msg_id[0];
           tx_buffer[6] = subMsg.msg_id[1];
           tx_buffer[7] = 0x00;             // Return code = Accepted
                     
           client_address = 256*subMsg.msg_id[0] + subMsg.msg_id[1];
                         
           Serial.print("Sending SUBACK back to ");
           Serial.println(client_address,HEX);
              
           delay(5000);
           mrf.send16((word)client_address,(char*)tx_buffer, tx_buffer[0]);  // Send SUBACK back
           
        } break;

        case(UNSUBSCRIBE):
        {
          Serial.println("UNSUBSCRIBE received. Not implemented yet.");
        } break;

        case(DISCONNECT):
        {
          Serial.println("DISCONNECT received. Not implemented yet.");
        } break;
        
      }
      message_received = false;
   }

// -------------------------Routines for gateway operations -------------------------

    current_time = millis();

    if(current_time - last_update > update_interval)
    {
      last_update = current_time;
      i=0;
      while(clients[i].address!=0xFFFF)
      {
        if(clients[i].client_connected == true)
        {
          clients[i].timer--;
//          Serial.print("Timeout in ");
//          Serial.print(clients[i].timer);
//          Serial.println(" s");
          if(clients[i].timer < 0)
          {
            Serial.print("Client 0x");
            Serial.print(clients[i].address,HEX);
            Serial.println(" timed out.");

//            mqttClient.publish((char *)clients[i].address,tout_msg);

            String(clients[i].address,HEX).toCharArray(conn_addr,5);
            mqttClient.publish(conn_addr,tout_msg); // Send message to MQTT broker for the new client connected
            
//            clients[i].client_connected = false;
//            clients[i].address = 0xFFFF;
                                              // find the last record and move it
                                              // into this position
                                              // to prevent holes
            for(move_counter=i;clients[move_counter].address!=0xFFFF;move_counter++);
//            while(clients[move_counter++].address != 0xFFFF); // find the end of the records
            move_counter--;                                  // get the latest record
            Serial.println(move_counter);
            clients[i].address = clients[move_counter].address;  // move the last record at the free place
            clients[move_counter].address = 0xFFFF;
            clients[move_counter].client_connected = false;
            
          }
        }
        i++;
      }

      for(i=0;clients[i].address!=0xFFFF;i++);
      Serial.print("Clients connected: ");
      Serial.println(i);
      if(i==0) clientsn[0] = '0';
      if(i==1) clientsn[0] = '1';
      if(i==2) clientsn[0] = '2';
      if(i==3) clientsn[0] = '3';
      if(i>3) clientsn[0] = 'U';
//      clientsn[1] = 0x00;
      mqttClient.publish("clientsn",(char *)clientsn);
    }

// ------------------------------------------------------------------------------

    if(mqtt_msg_received)
    {

      uint16_t dest_address;
      
      Serial.print("Topic: ");
      Serial.println((char *)mqtt_rx_topic);
      Serial.print("Payload: ");
      Serial.println((char *)mqtt_rx_payload);
          
      i=0;
      while((sub_table[i].address[0] != 0xFF) && (sub_table[i].address[1] != 0xFF))
      {
        if((sub_table[i].topic_id[0] == mqtt_rx_topic[0]) && (sub_table[i].topic_id[1] == mqtt_rx_topic[1]))
        {  
          tx_buffer[1] = PUBLISH;
          tx_buffer[2] = 0x00;
          tx_buffer[3] = mqtt_rx_topic[0];
          tx_buffer[4] = mqtt_rx_topic[1];
          tx_buffer[5] = sub_table[i].address[0];
          tx_buffer[6] = sub_table[i].address[1];

          j=0;
          while(mqtt_rx_payload[j])
          {
            tx_buffer[6+j] = mqtt_rx_payload[j];
            j++;
          }
          tx_buffer[0] = 6+j; // total msg len

//          Serial.println(sub_table[i].address[0],HEX);
//          Serial.println(sub_table[i].address[1],HEX);
          
          dest_address = sub_table[i].address[0] << 8;
//          Serial.println(dest_address,HEX);
          dest_address = dest_address + sub_table[i].address[1];
//          Serial.println(dest_address,HEX);

          mrf.send16(dest_address,(char *)tx_buffer,6+j); // TODO Construct destination address
        }
      i++;    
      }    
      mqtt_msg_received = false;
    }

   mqttClient.loop();
}

void handle_rx() {

    uint8_t i;
    
    digitalWrite(LED_BUILTIN, LOW);
//    Serial.println("Packet received, waiting 100 msec...");
//    delay(500);
    rx_len = mrf.rx_datalength();
    for (i = 0; i < rx_len; i++)
    {
//          rx_buffer[i] = mrf.get_rxbuf()[i];
          rx_buffer[i] = mrf.get_rxinfo()->rx_data[i];
//          delay(100);
    }
    message_received = true;
    digitalWrite(LED_BUILTIN, HIGH);
}

void handle_tx() {
//    if (mrf.get_txinfo()->tx_ok) {
//        Serial.println("TX went ok, got ack");
//    } else {
//        Serial.print("TX failed after ");Serial.print(mrf.get_txinfo()->retries);Serial.println(" retries\n");
//    }
}

void callback(char* topic, byte* payload, unsigned int length) {

  uint8_t i;
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  if(length>15)
  {
    Serial.println("Received message too long");
  }
  else
    {
      for(i=0;i<2;i++)
        mqtt_rx_topic[i] = topic[i];
      mqtt_rx_topic[i] = 0x00;
      
      for (i=0;i<length;i++)
      {
        Serial.print((char)payload[i]);
        mqtt_rx_payload[i] = payload[i];
      }
      mqtt_rx_payload[i] = 0x00;
      mqtt_msg_received = true;
    }
  Serial.println();
  }
