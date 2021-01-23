# MQTT-SN to MQTT Gateway

MQTT-SN to MQTT Gateway implementation based on MRF24J40 ESP12 NodeMCU with Arduino code

Visit project website at https://sites.google.com/view/hobbyiot/projects/mqtt-sn-802-15-4

Twitter: @sivanovbg

This is an MQTT-SN to MQTT Gateway implementation based on MRF24J40 and NodeMCU with Arduino code
The implementation is based ot MQTT-SN Specification Version 1.2.
An ESP12 NodeMCU module is used for both executing the Gateway functions and connecting to the Internet over WiFi.

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

!(HIoT GW PCB Parts No Box.jpg)

