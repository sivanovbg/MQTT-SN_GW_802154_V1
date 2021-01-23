##HobbyIoT MQTT-SN to MQTT Gateway

##Summary

HobbyIoT NET is an Open source wireless sensor network adventure. The solution concept was developed around the general idea to have a fully open source, easy to program and upgrade, easy to implement and cheap IoT solution.

All modules are Arduino compatible. The connectivity is based on the low power 802.15.4 transport running the MQTT-SN protocol right over it to have the simplest and shortest messages possible. The Gateway supports up to 1024 end devices and translates messages to and from standard MQTT protocol.

We are currently looking into the opportunity to join the "Works with Arduino" partner program.

The HobbyIoT NET is an MQTT-SN protocol implementation over 802.15.4 physical connection. It connects together number of End devices - MQTT-SN Clients - capable to measure and control various parameters and appliances. Each End device connects to the MQTT-SN Gateway and exchange information over it with the MQTT Server. This server can either just pass the information or - if programmed - make decisions how to manage the sensor network by itself.

##Resources

Gateway project website: https://www.hobbyiot.net/technology/gateway-operation
HobbyIoT NET website: https://www.hobbyiot.net/

GitHub: https://github.com/sivanovbg/MQTT-SN_GW_802154_V1

Circuit Maker project: https://circuitmaker.com/Projects/Details/sivanovbg/HIoTGWV1

Order board at OSH Park: https://oshpark.com/shared_projects/sBSpwQuP

MQTT-SN Specification: http://mqtt.org/new/wp-content/uploads/2009/06/MQTT-SN_spec_v1.2.pdf

Twitter, Slack: #HobbyIoT

##Introduction

<p align="center">
	<img src="https://github.com/sivanovbg/https://github.com/sivanovbg/https://github.com/sivanovbg/MQTT-SN_GW_802154_V1/blob/master/HIoT%20GW%20PCB%20Parts%20No%20Box.jpg?raw=true" width="50%" />
</p>

The HobbyIot NET Gateway is built around a ESP8266 NodeMCU and MRF24J40MA radio module. They are connected together via SPI interface. The Gateway is connected to the MQTT Broker (Raspberry Pi running Mosquitto for example) over the WiFi interface of the ESP8266 NodeMCU. The sensor side of the bridge communicates with the end devices over the 2.4 GHz 802.15.4 interface via MRF24J40MA running the MQTT-SN protocol.

The code is written in Arduino environment and is available on GitHub under Open source MIT License.

The Algorithm

<p align="center">
	<img src="https://github.com/sivanovbg/https://github.com/sivanovbg/MQTT-SN_GW_802154_V1/blob/master/HIoT%20Gateway%20State%20diagram.png?raw=true" width="50%" />
</p>

After Power on or reset applied to the system both MQTT and MQTT-SN sides are first initialized. The MQTT setup includes a WiFi setup and MQTT connection procedure. The MQTT-SN setup starts with 802.15.4 module and connection parameters set up.

The state diagram describes the MQTT-SN operation of the Bridge including message reception, message processing and data forwarding to and from the MQTT side of the bridge.

#Operation

Once the system is initialized the Gateway is waiting for a message. The main routine (bold arrows) waits for a message to receive. It also checks the state of the modes in the ConnTable.

The state diagram shows the Sensor network side of the Algorithm operation.

In case of messages received on the MQTT-SN side it is first classified according to its type.

In case of valid message (implemented message) a certain actions are activated according the message type:

In case of a CONN message the corresponding node is added to the ConnTable and a CONNACK is sent back; If the node is already in the connection table a CONNACK is just sent back.

In case of a PINGREQ message a PINGRESP is transmitted back the the ping requesting node. The corresponding time-out timer is reset;

In case of SUB message the corresponding topic is added to the SubTable and a SUBACK is transmitted back to the requesting node;

In case of PUB message the corresponding topic and data are transmitted to the MQTT side; If the PUB comes from the MQTT the data is forwarded to the corresponding node (its address should be contained within the message)

In case of invalid (or still not implemented one) the system goes back to wait for a valid message.

The algorithm also periodically checks the ConnTable for time-outed nodes and removes them from the table.

For more information and details, please visit the GitHub repo of the project or connect over the #HobbyIoT Slack channel.

##Versions

Version Alpha 1 (VA1)

This is a very early implementation. Part of the whole message list according specs is supported so far.
Topics should be predefined on the client and gateway sides and are fixed to 2 positions alpha-numeric string.

Version 1 (V1)

This is the recent implementation. Most of the bugs from VA1 are fixed now. Part of the whole message list according specs is supported so far.
Topics should be predefined on the client and gateway sides and are fixed to 2 positions alpha-numeric string. Enjoy!

Messages supported:

CONNECT
CONNACK
PINGREQ
PINGRESP
PUBLISH (both directions)
SUBSCRIBE
SUBACK
