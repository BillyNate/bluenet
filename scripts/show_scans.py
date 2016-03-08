#!/usr/bin/python

#from pylab import *
import matplotlib.pyplot as plt
import sys
import time
import datetime
import re
import os
import cPickle as pickle
import json

"""
Parses a (timestamped!) minicom capture and plots graphs
At the bottom, select what to plot
"""

beaconNames = {
	"E3 BF 7F 5F 59 0C":"kontakt 1",
	"DF 8F 3F A7 A0 BE":"kontakt 2",
	"C0 B7 12 98 FD 7C":"kontakt 3",

	"E6 EF B4 98 25 4B":"usb14",
	"E5 6F 56 1F 97 83":"usb54",
	"C4 D0 90 6F 53 4B":"usb56",
	"D0 E8 42 71 C2 5D":"usb76",
	"D0 6D AA 45 54 55":"usb91",

	"F8 27 73 28 DA FE":"hans",
	"F1 C1 B8 AC 03 CD":"chloe",
	"E1 89 95 C1 06 04":"meeting",
	"DB 26 1F D9 FA 5E":"old office chloe",
	"F0 20 A1 2C 57 D4":"hallway 0",
	"C9 92 1A 78 F4 81":"trash room",
	"CF 5E 84 EF 00 91":"lunch room",
	"ED F5 F8 E3 6A F6":"kitchen",
	"EB 4D 30 14 6D C1":"server room",
	"F4 A2 89 23 53 92":"basement",
	"EB 82 34 DA EE 0B":"balcony",
	"ED AF F3 7E E1 47":"jan geert",
	"C6 27 A8 D7 D4 C7":"allert",
	"E0 31 D7 C5 CA FF":"hallway 1",
	"D7 59 D6 BD 2A 5A":"dobots software",
	"DE 41 8E 2F 58 85":"interns",
	"D7 D5 51 82 49 43":"peet",
	"FD CB 99 58 0B 88":"hallway 2",
	"EF 36 60 78 1F 1D":"dobots hardware",
	"E8 B7 37 29 F4 77":"hallway 3",
	"C5 25 3F 5E 92 6F":"small multi purpose room",
	"EA A6 ED 8A 13 8E":"billiard table",
	"C5 71 64 3A 15 74":"proto table",

	"DC 1A 5A AF 1A 44":"hans",
	"C6 A0 0C 5A C8 C6":"chloe",
	"D4 0B 9E 30 B2 73":"meeting",
	"CC 02 60 7A 83 46":"old office chloe",
	"D5 A7 34 EC 72 90":"hallway 0",
	"DB E3 E7 9B 60 81":"trash room",
	"E8 DF 9A 90 54 6C":"lunch room",
	"C4 D9 1C 34 16 09":"kitchen",
	"C0 82 3E B9 F5 91":"server room",
	"D7 BE C6 21 71 F2":"basement",
	"EB C9 C2 58 52 C4":"balcony",
	"C2 92 09 5F 04 78":"jan geert",
	"F8 85 28 C1 4D D2":"allert",
	"C6 B1 9A 0A 25 E0":"hallway 1",
	"F8 AC E5 C4 E0 3E":"dobots software",
	"FA 23 55 F1 04 0E":"interns",
	"F5 A7 4B 49 8C 7D":"peet",
	"C0 6E 12 A4 6B 06":"hallway 2",
	"EF 79 08 EF 50 AC":"dobots hardware",
	"E8 00 93 4E 7B D9":"hallway 3",
	"E8 C5 AE A7 6B A9":"small multi purpose room",
	"EA 64 68 47 B8 EE":"billiard table",
	"FE 04 85 F9 8F E9":"proto table",
	"EC 9C 70 56 9F 90":"32k",
}

def parse(filename):
	"""
	Parses a log file and returns the parsed data
	:param filename:
	:return:
	data["scans"]
		scans["node address"] = [entry, entry, ...]
			entry["time"] = timestamp
			entry["address"] = address
			entry["rssi"] = rssi
			entry["occurances"] = occurances
	data["startTimestamp"] = first seen timestamp
	data["endTimestamp"] = last seen timestamp
	"""
	scanning = False
	logfile = open(filename, "r")

	# Search for something like:
	# [2015-12-24 11:23:16] [cs_MeshControl.cpp             : 68   ] Device D0 E8 42 71 C2 5D scanned these devices:
	startPattern = re.compile("\\[([0-9 \\-:]+)\\] .* Device ([0-9A-Z ]+) scanned these devices:")

	# Search for something like:
	# [2015-12-24 11:23:16] [cs_MeshControl.cpp             : 79   ] 2: [C4 D0 90 6F 53 4B]   rssi:  -51    occ:  10
	scanPattern = re.compile("\\[([0-9 \\-:]+)\\] .* [0-9]+: \\[([0-9A-Z ]+)\\]\\s+rssi:\\s+(-?[0-9]+)\\s+occ:\\s+([0-9]+)")

	# scans: map with:
	#	scans["node address"] = [entry, entry, ...]
	#		entry["time"] = timestamp
	#		entry["address"] = address
	#		entry["rssi"] = rssi
	#		entry["occurances"] = occurances
	#	scans["startTimestamp"] = first seen timestamp
	#	scans["endTimestamp"] = last seen timestamp

	nodes = []
	scans = {}
	data = {"scans" : scans}

	startFound = False
	startTimestamp = -1
	endTimestamp = -1
	address = ""
	for line in logfile:
		matches = startPattern.findall(line)
		if len(matches):
			timestamp = time.mktime(datetime.datetime.strptime(matches[0][0], "%Y-%m-%d %H:%M:%S").timetuple())
			if (startTimestamp < 0):
				startTimestamp = timestamp
			address = matches[0][1]
			if (address not in nodes):
				nodes.append(address)

			startFound = True
#			print matches

		if (startFound):
			matches = scanPattern.findall(line)
			if len(matches):
#				startFound = False
				timestamp = time.mktime(datetime.datetime.strptime(matches[0][0], "%Y-%m-%d %H:%M:%S").timetuple())
				endTimestamp = timestamp
				entry = {"time":timestamp, "address":matches[0][1], "rssi":matches[0][2], "occurances":matches[0][3]}
				if (address not in scans):
					scans[address] = [entry]
				else:
					scans[address].append(entry)
#				print matches

	logfile.close()

#	print nodes
	print "Nodes:"
	for addr in nodes:
		scannerName = addr
		if (addr in beaconNames):
			scannerName = beaconNames[addr]
		print scannerName

#	print scans
	data["startTimestamp"] = startTimestamp
	data["endTimestamp"] = endTimestamp
	return data


def plotScansAsDots(data):
	scans = data["scans"]
	startTimestamp = data["startTimestamp"]
	endTimestamp = data["endTimestamp"]
	i=0
	for addr in scans:
		print len(scans[addr])
		plt.figure(i)
		timestamps = {}
		for scan in scans[addr]:
			dev = scan["address"]
			if (dev not in timestamps):
				timestamps[dev] = [scan["time"] - startTimestamp]
			else:
				timestamps[dev].append(scan["time"] - startTimestamp)
		j=0
		for device in timestamps:
			plt.plot(timestamps[device], [j]*len(timestamps[device]), ".")
			j+=1
		names = []
		for device in timestamps:
			if (device in beaconNames):
				names.append(beaconNames[device])
			else:
				names.append(device)

		plt.yticks(range(0,j), names)
		scannerName = addr
		if (addr in beaconNames):
			scannerName = beaconNames[addr]
		plt.title("Devices scanned by " + scannerName)
		plt.axis([0, endTimestamp-startTimestamp, -0.5, j-0.5])
		i+=1


def plotScansAsDots2(data):
	scans = data["scans"]
	startTimestamp = data["startTimestamp"]
	endTimestamp = data["endTimestamp"]
	plt.figure()
	i=0
	names = []
	for nodeAddr in scans:
#		print len(scans[nodeAddr])
#		plt.figure(i)
		timestamps = []
		for scan in scans[nodeAddr]:
			timestamps.append(scan["time"] - startTimestamp)
		plt.plot(timestamps, [i]*len(timestamps), ",")

		nodeName = nodeAddr
		if (nodeAddr in beaconNames):
			nodeName = beaconNames[nodeAddr]
		names.append(nodeName)
		i+=1

	plt.yticks(range(0,i), names)
	plt.title("Scan nodes")
	plt.axis([0, endTimestamp-startTimestamp, -0.5, i-0.5])


def plotBandwidth(data):
	# TODO: last value of numScans is wrong because that timeslot can be less than dt

	dt = 60

	scans = data["scans"]
	startTimestamp = data["startTimestamp"]
	endTimestamp = data["endTimestamp"]
	startTimes = range(int(startTimestamp), int(endTimestamp)+dt, dt)
#	print startTimes
	numScans = {}
	numScans["total"] = [0]*len(startTimes)

	lineColors = ["b", "g", "r", "c", "m", "y", "k"]
	lineStyles = ["-o", "-^", "-d", "-+", "-*"]

	plt.figure()
	i = 0
	for node in scans:
		numScans[node] = [0]*len(startTimes)
		timeInd = 0
		for scan in scans[node]:
			scanTime = int(scan["time"])
			if (scanTime > startTimes[timeInd+1]):
				timeInd += 1
#			numScans[timeInd] += 1
			numScans[node][timeInd] += 1.0 / dt
			numScans["total"][timeInd] += 1.0 / dt
			scannerName = node
			if (node in beaconNames):
				scannerName = beaconNames[node]
		plt.title("Number of scanned devices per second")
		fmt = lineColors[i % len(lineColors)] + lineStyles[int(i/len(lineColors)) % len(lineStyles)]
		plt.plot(startTimes[0:-1], numScans[node][0:-1], fmt, label=scannerName)
		i+=1
	plt.legend(loc="upper left")

	plt.figure()
	plt.title("Total number of scanned devices per second")
	plt.plot(startTimes[0:-1], numScans["total"][0:-1])

def plotBandwidth2(data):
	# Time step
	dt = 60

	# Averaging window: must be divisible by 2*dt
	window = 300

	scans = data["scans"]
	startTimestamp = data["startTimestamp"]
	endTimestamp = data["endTimestamp"]
	startTimes = range(int(startTimestamp), int(endTimestamp)+dt, dt)
#	print startTimes
	numScans = {}
	numScans["total"] = [0]*len(startTimes)

	plt.figure()
	for node in scans:
		numScans[node] = [0]*len(startTimes)
		timeInd = 0
		for scan in scans[node]:
			scanTime = int(scan["time"])

			if (scanTime > startTimes[timeInd+1]):
				timeInd += 1

			tIndStart = max(timeInd - window/2/dt, 0)
			tIndEnd = min(timeInd + window/2/dt, len(startTimes)-1)
			for tInd in range(tIndStart, tIndEnd):
				numScans[node][tInd] += 1.0
				numScans["total"][tInd] += 1.0

		for timeInd in range(0, len(startTimes)):
			tIndStart = max(timeInd - window/2/dt, 0)
			tIndEnd = min(timeInd + window/2/dt, len(startTimes)-1)
			actualWindowSize = startTimes[tIndEnd] - startTimes[tIndStart]
			numScans[node][timeInd] /= actualWindowSize

		plt.title("Number of scanned devices per second (moving average)")
		plt.plot(startTimes[0:-1], numScans[node][0:-1])

	for timeInd in range(0, len(startTimes)):
		tIndStart = max(timeInd - window/2/dt, 0)
		tIndEnd = min(timeInd + window/2/dt, len(startTimes)-1)
		actualWindowSize = startTimes[tIndEnd] - startTimes[tIndStart]
		numScans["total"][timeInd] /= actualWindowSize

	plt.figure()
	plt.title("Total number of scanned devices per second (moving average)")
	plt.plot(startTimes[0:-1], numScans["total"][0:-1])


if __name__ == '__main__':
	fileName = sys.argv[1]
	fileBaseName, fileExtension = os.path.splitext(fileName)
	if (fileExtension == ".p"):
		with open(fileName, "r") as fp:
			logData = pickle.load(fp)
	elif (fileExtension == ".json"):
		with open("filename", "r") as fp:
			logData = json.load(fp)
	else:
		logData = parse(fileName)
		with open(fileBaseName + ".p", "wb") as fp:
			pickle.dump(logData, fp, pickle.HIGHEST_PROTOCOL)
#		with open(fileBaseName + ".json", "w") as fp:
#			json.dump(logData, fp)


#	plotScansAsDots(logData)
	plotScansAsDots2(logData)
	plotBandwidth(logData)
#	plotBandwidth2(logData)
	plt.show()






