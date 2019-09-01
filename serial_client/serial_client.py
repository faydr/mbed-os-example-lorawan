import serial
import sys, os
import logging
import json
import base64


# Open the serial port
serial_port = sys.argv[1]
ser = serial.Serial(serial_port, baudrate=921600)

while True:
    line = ser.readline().decode('utf-8')
    parsed_line = {}
    parsed_line["Type"] = ""
    try: 
        parsed_line = json.loads(line)
    except Exception as e:
        print("\033[1;31;40m" + str(line[:-2]) + "\033[1;37;40m")
    if parsed_line["Type"] == "Debug Msg":
        msg = base64.b64decode(parsed_line["Message"]).decode('utf-8')
        print("\033[1;32;40m" + str(msg[:-2]) + "\033[1;37;40m")
    elif parsed_line["Type"] == "Status":
        status = parsed_line["Status"]
        value = parsed_line["Value"]
        print("Status Msg: %s, %s" % (status, value))
    elif parsed_line["Type"] == "Settings":
        freq = parsed_line["Freq"]
        sf = parsed_line["SF"]
        bw = parsed_line["BW"]
        cr = parsed_line["CR"]
        mode = parsed_line["Mode"]
    elif parsed_line["Type"] == "Frame":
        frame["HDR Pkt Type"] = parsed_line["HDR Pkt Type"]
        frame["HDR Stream ID"] = parsed_line["HDR Stream ID"]
        frame["HDR TTL"] = parsed_line["HDR TTL"]
        frame["HDR Sender"] = parsed_line["HDR Sender"]
        frame["HDR Pre Offset"] = parsed_line["HDR Pre Offset"]
        frame["HDR Num Sym Offset"] = parsed_line["HDR Num Sym Offset"]
        frame["HDR Sym Offset"] = parsed_line["HDR Sym Offset"]
        frame["Header CRC"] = parsed_line["Header CRC"]
        frame["Data CRC"] = parsed_line["Data CRC"]
        frame["Data Payload"] = base64.base64decode(frame["Data Payload"])
