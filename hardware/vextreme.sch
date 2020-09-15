EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "VEXTREME Vectrex Multicart"
Date "2020-09-11"
Rev "v0.4"
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L vextreme:GND #PWR07
U 1 1 59F16914
P 1700 5300
F 0 "#PWR07" H 1700 5050 50  0001 C CNN
F 1 "GND" H 1700 5150 50  0000 C CNN
F 2 "" H 1700 5300 50  0001 C CNN
F 3 "" H 1700 5300 50  0001 C CNN
	1    1700 5300
	1    0    0    -1  
$EndComp
Text GLabel 950  2100 0    60   Input ~ 0
V-HALT
Text GLabel 950  2200 0    60   Input ~ 0
V-OE
Text GLabel 950  2300 0    60   Input ~ 0
V-CE
Text GLabel 950  2400 0    60   Input ~ 0
V-RW
Text GLabel 950  2500 0    60   Input ~ 0
V-CART
Text GLabel 950  2600 0    60   Input ~ 0
V-NMI
Text GLabel 950  2700 0    60   Input ~ 0
V-PB6
Text GLabel 950  2800 0    60   Input ~ 0
V-IRQ
Wire Wire Line
	1700 5150 1700 5200
Wire Wire Line
	1700 5200 1800 5200
Wire Wire Line
	1800 5200 1800 5150
Connection ~ 1700 5200
Wire Wire Line
	1900 5200 1900 5150
Connection ~ 1800 5200
Wire Wire Line
	1750 1450 1850 1450
Wire Wire Line
	1850 1450 1850 1500
Connection ~ 1750 1450
Wire Wire Line
	1100 2200 950  2200
Wire Wire Line
	1100 2300 950  2300
Wire Wire Line
	1100 2400 950  2400
Wire Wire Line
	1100 2500 950  2500
Wire Wire Line
	1100 2600 950  2600
Wire Wire Line
	1100 2800 950  2800
Wire Wire Line
	1700 5200 1700 5300
Wire Wire Line
	1800 5200 1900 5200
Wire Wire Line
	1750 1450 1750 1500
$Comp
L vextreme:LM1117-3.3 U1
U 1 1 5D0000A1
P 4250 5900
F 0 "U1" H 4250 6142 50  0000 C CNN
F 1 "LM1117-3.3" H 4250 6051 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-223-3_TabPin2" H 4250 5900 50  0001 C CNN
F 3 "http://www.ti.com/lit/ds/symlink/lm1117.pdf" H 4250 5900 50  0001 C CNN
	1    4250 5900
	1    0    0    -1  
$EndComp
Wire Wire Line
	2350 4450 3550 4450
Wire Wire Line
	3550 4350 2350 4350
Wire Wire Line
	2350 4250 3550 4250
Wire Wire Line
	3550 4150 2350 4150
Wire Wire Line
	2350 4050 3550 4050
Wire Wire Line
	3550 3950 2350 3950
Wire Wire Line
	2350 3850 3550 3850
Wire Wire Line
	3550 3750 2350 3750
Wire Wire Line
	2350 3650 3550 3650
Wire Wire Line
	3550 3550 2350 3550
Wire Wire Line
	2350 3450 3550 3450
Wire Wire Line
	3550 3350 2350 3350
Wire Wire Line
	2350 3250 3550 3250
Wire Wire Line
	3550 3150 2350 3150
Wire Wire Line
	2350 3050 3550 3050
Text GLabel 3550 4550 0    60   Input ~ 0
V-PB6
Wire Wire Line
	1100 2700 950  2700
$Comp
L vextreme:C C1
U 1 1 5D00D362
P 3850 6200
F 0 "C1" H 3965 6246 50  0000 L CNN
F 1 "2.2u" H 3965 6155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 3888 6050 50  0001 C CNN
F 3 "~" H 3850 6200 50  0001 C CNN
	1    3850 6200
	1    0    0    -1  
$EndComp
$Comp
L vextreme:C C2
U 1 1 5D00E0E5
P 4600 6200
F 0 "C2" H 4715 6246 50  0000 L CNN
F 1 "2.2u" H 4715 6155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 4638 6050 50  0001 C CNN
F 3 "~" H 4600 6200 50  0001 C CNN
	1    4600 6200
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0101
U 1 1 5D00E463
P 3850 6350
F 0 "#PWR0101" H 3850 6100 50  0001 C CNN
F 1 "GND" H 3850 6200 50  0000 C CNN
F 2 "" H 3850 6350 50  0001 C CNN
F 3 "" H 3850 6350 50  0001 C CNN
	1    3850 6350
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0102
U 1 1 5D00E892
P 4600 6350
F 0 "#PWR0102" H 4600 6100 50  0001 C CNN
F 1 "GND" H 4600 6200 50  0000 C CNN
F 2 "" H 4600 6350 50  0001 C CNN
F 3 "" H 4600 6350 50  0001 C CNN
	1    4600 6350
	1    0    0    -1  
$EndComp
Wire Wire Line
	4250 6200 4250 6350
$Comp
L vextreme:GND #PWR0103
U 1 1 5D00F858
P 4250 6350
F 0 "#PWR0103" H 4250 6100 50  0001 C CNN
F 1 "GND" H 4250 6200 50  0000 C CNN
F 2 "" H 4250 6350 50  0001 C CNN
F 3 "" H 4250 6350 50  0001 C CNN
	1    4250 6350
	1    0    0    -1  
$EndComp
Wire Wire Line
	3850 6050 3850 5900
Wire Wire Line
	4550 5900 4600 5900
Wire Wire Line
	4600 5900 4600 6050
Connection ~ 4600 5900
$Comp
L vextreme:MBR0520 D1
U 1 1 5D013F81
P 3500 5900
F 0 "D1" H 3500 5684 50  0000 C CNN
F 1 "MBR0520" H 3500 5775 50  0000 C CNN
F 2 "Diode_SMD:D_SOD-123" H 3500 5725 50  0001 C CNN
F 3 "http://www.mccsemi.com/up_pdf/MBR0520~MBR0580(SOD123).pdf" H 3500 5900 50  0001 C CNN
	1    3500 5900
	-1   0    0    1   
$EndComp
Wire Wire Line
	3650 5900 3850 5900
Connection ~ 3850 5900
Wire Wire Line
	3850 5900 3950 5900
$Comp
L vextreme:MBR0520 D2
U 1 1 5D017920
P 1350 1200
F 0 "D2" V 1304 1279 50  0000 L CNN
F 1 "MBR0520" V 1395 1279 50  0000 L CNN
F 2 "Diode_SMD:D_SOD-123" H 1350 1025 50  0001 C CNN
F 3 "http://www.mccsemi.com/up_pdf/MBR0520~MBR0580(SOD123).pdf" H 1350 1200 50  0001 C CNN
	1    1350 1200
	0    1    1    0   
$EndComp
Text GLabel 4950 2350 2    50   Input ~ 0
USB-5V
$Comp
L vextreme:GND #PWR0105
U 1 1 5D01B9D7
P 5800 4750
F 0 "#PWR0105" H 5800 4500 50  0001 C CNN
F 1 "GND" H 5800 4600 50  0000 C CNN
F 2 "" H 5800 4750 50  0001 C CNN
F 3 "" H 5800 4750 50  0001 C CNN
	1    5800 4750
	1    0    0    -1  
$EndComp
Wire Wire Line
	5800 4650 5550 4650
Wire Wire Line
	5550 4250 7100 4250
Wire Wire Line
	7100 4250 7100 4450
Wire Wire Line
	7100 4450 7100 4550
Connection ~ 7100 4450
Text GLabel 5800 4450 0    50   Input ~ 0
CSn
Text GLabel 5800 4550 0    50   Input ~ 0
DO
Text GLabel 7100 4750 2    50   Input ~ 0
DI
$Comp
L vextreme:C C4
U 1 1 5D022EBD
P 5550 4900
F 0 "C4" H 5300 4950 50  0000 L CNN
F 1 "100nF" H 5200 4850 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 5588 4750 50  0001 C CNN
F 3 "~" H 5550 4900 50  0001 C CNN
	1    5550 4900
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0106
U 1 1 5D023148
P 5550 5050
F 0 "#PWR0106" H 5550 4800 50  0001 C CNN
F 1 "GND" H 5550 4900 50  0000 C CNN
F 2 "" H 5550 5050 50  0001 C CNN
F 3 "" H 5550 5050 50  0001 C CNN
	1    5550 5050
	1    0    0    -1  
$EndComp
Connection ~ 7100 4250
Wire Wire Line
	7100 4250 7100 4150
$Comp
L vextreme:+3.3V #PWR0107
U 1 1 5D025C55
P 7100 4150
F 0 "#PWR0107" H 7100 4000 50  0001 C CNN
F 1 "+3.3V" H 7115 4323 50  0000 C CNN
F 2 "" H 7100 4150 50  0001 C CNN
F 3 "" H 7100 4150 50  0001 C CNN
	1    7100 4150
	1    0    0    -1  
$EndComp
Text GLabel 4950 3550 2    50   Input ~ 0
DO
Text GLabel 7100 4650 2    50   Input ~ 0
CLK
Text GLabel 4950 3450 2    50   Input ~ 0
CLK
Text GLabel 4950 3650 2    50   Input ~ 0
DI
Text GLabel 4950 3250 2    60   Input ~ 0
V-RW
Text GLabel 2350 2100 2    50   Input ~ 0
C_D7
Text GLabel 2350 2200 2    50   Input ~ 0
C_D6
Text GLabel 2350 2300 2    50   Input ~ 0
C_D5
Text GLabel 2350 2400 2    50   Input ~ 0
C_D4
Text GLabel 2350 2500 2    50   Input ~ 0
C_D3
Text GLabel 2350 2600 2    50   Input ~ 0
C_D2
Text GLabel 2350 2700 2    50   Input ~ 0
C_D1
Text GLabel 2350 2800 2    50   Input ~ 0
C_D0
Text GLabel 7200 1450 2    50   Input ~ 0
C_D7
Text GLabel 7200 1550 2    50   Input ~ 0
C_D6
Text GLabel 7200 1650 2    50   Input ~ 0
C_D5
Text GLabel 7200 1750 2    50   Input ~ 0
C_D4
Text GLabel 7200 1850 2    50   Input ~ 0
C_D3
Text GLabel 7200 1950 2    50   Input ~ 0
C_D2
Text GLabel 7200 2050 2    50   Input ~ 0
C_D1
Text GLabel 7200 2150 2    50   Input ~ 0
C_D0
Text GLabel 5800 2350 0    60   Input ~ 0
V-RW
Wire Wire Line
	6400 2750 6500 2750
Connection ~ 6500 2750
Wire Wire Line
	6500 2750 6600 2750
Wire Wire Line
	6500 2750 6500 2850
$Comp
L vextreme:GND #PWR0108
U 1 1 5D040111
P 6500 2850
F 0 "#PWR0108" H 6500 2600 50  0001 C CNN
F 1 "GND" H 6505 2677 50  0000 C CNN
F 2 "" H 6500 2850 50  0001 C CNN
F 3 "" H 6500 2850 50  0001 C CNN
	1    6500 2850
	1    0    0    -1  
$EndComp
$Comp
L vextreme:C C3
U 1 1 5D041D40
P 5650 950
F 0 "C3" H 5765 996 50  0000 L CNN
F 1 "100nF" H 5700 850 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 5688 800 50  0001 C CNN
F 3 "~" H 5650 950 50  0001 C CNN
	1    5650 950 
	1    0    0    -1  
$EndComp
Wire Wire Line
	6300 1150 6300 800 
Wire Wire Line
	6300 800  5650 800 
$Comp
L vextreme:GND #PWR0109
U 1 1 5D046114
P 5650 1100
F 0 "#PWR0109" H 5650 850 50  0001 C CNN
F 1 "GND" H 5655 927 50  0000 C CNN
F 2 "" H 5650 1100 50  0001 C CNN
F 3 "" H 5650 1100 50  0001 C CNN
	1    5650 1100
	1    0    0    -1  
$EndComp
$Comp
L vextreme:+3.3V #PWR0110
U 1 1 5D05BBF8
P 5650 750
F 0 "#PWR0110" H 5650 600 50  0001 C CNN
F 1 "+3.3V" H 5665 923 50  0000 C CNN
F 2 "" H 5650 750 50  0001 C CNN
F 3 "" H 5650 750 50  0001 C CNN
	1    5650 750 
	1    0    0    -1  
$EndComp
Connection ~ 5650 800 
Wire Wire Line
	6600 1150 6650 1150
Wire Wire Line
	6650 1150 6650 800 
Connection ~ 6650 1150
Wire Wire Line
	6650 1150 6700 1150
$Comp
L vextreme:C C5
U 1 1 5D05F58F
P 7350 950
F 0 "C5" H 7465 996 50  0000 L CNN
F 1 "100nF" H 7465 905 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 7388 800 50  0001 C CNN
F 3 "~" H 7350 950 50  0001 C CNN
	1    7350 950 
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0111
U 1 1 5D05F938
P 7350 1100
F 0 "#PWR0111" H 7350 850 50  0001 C CNN
F 1 "GND" H 7355 927 50  0000 C CNN
F 2 "" H 7350 1100 50  0001 C CNN
F 3 "" H 7350 1100 50  0001 C CNN
	1    7350 1100
	1    0    0    -1  
$EndComp
$Comp
L vextreme:+5V #PWR0112
U 1 1 5D05FE50
P 7350 750
F 0 "#PWR0112" H 7350 600 50  0001 C CNN
F 1 "+5V" H 7350 900 50  0000 C CNN
F 2 "" H 7350 750 50  0001 C CNN
F 3 "" H 7350 750 50  0001 C CNN
	1    7350 750 
	1    0    0    -1  
$EndComp
Connection ~ 7350 800 
Wire Wire Line
	4150 1250 4250 1250
Connection ~ 4250 1250
Wire Wire Line
	4250 1250 4350 1250
Connection ~ 4350 1250
Wire Wire Line
	4350 1250 4450 1250
Connection ~ 4450 1250
$Comp
L vextreme:C C10
U 1 1 5D0654C6
P 4600 950
F 0 "C10" H 4715 996 50  0000 L CNN
F 1 "100nF" H 4650 850 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 4638 800 50  0001 C CNN
F 3 "~" H 4600 950 50  0001 C CNN
	1    4600 950 
	1    0    0    -1  
$EndComp
$Comp
L vextreme:C C11
U 1 1 5D065855
P 4950 950
F 0 "C11" H 5065 996 50  0000 L CNN
F 1 "100nF" H 5000 850 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 4988 800 50  0001 C CNN
F 3 "~" H 4950 950 50  0001 C CNN
	1    4950 950 
	1    0    0    -1  
$EndComp
$Comp
L vextreme:C C12
U 1 1 5D065FA9
P 5300 950
F 0 "C12" H 5415 996 50  0000 L CNN
F 1 "100nF" H 5350 850 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 5338 800 50  0001 C CNN
F 3 "~" H 5300 950 50  0001 C CNN
	1    5300 950 
	1    0    0    -1  
$EndComp
Connection ~ 4600 800 
Wire Wire Line
	5300 800  5300 750 
$Comp
L vextreme:+3.3V #PWR0113
U 1 1 5D06A644
P 5300 750
F 0 "#PWR0113" H 5300 600 50  0001 C CNN
F 1 "+3.3V" H 5315 923 50  0000 C CNN
F 2 "" H 5300 750 50  0001 C CNN
F 3 "" H 5300 750 50  0001 C CNN
	1    5300 750 
	1    0    0    -1  
$EndComp
Wire Wire Line
	5650 750  5650 800 
$Comp
L vextreme:GND #PWR0114
U 1 1 5D06DD33
P 5300 1100
F 0 "#PWR0114" H 5300 850 50  0001 C CNN
F 1 "GND" H 5305 927 50  0000 C CNN
F 2 "" H 5300 1100 50  0001 C CNN
F 3 "" H 5300 1100 50  0001 C CNN
	1    5300 1100
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0115
U 1 1 5D06E1CA
P 4950 1100
F 0 "#PWR0115" H 4950 850 50  0001 C CNN
F 1 "GND" H 4955 927 50  0000 C CNN
F 2 "" H 4950 1100 50  0001 C CNN
F 3 "" H 4950 1100 50  0001 C CNN
	1    4950 1100
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0116
U 1 1 5D06E51F
P 4600 1100
F 0 "#PWR0116" H 4600 850 50  0001 C CNN
F 1 "GND" H 4605 927 50  0000 C CNN
F 2 "" H 4600 1100 50  0001 C CNN
F 3 "" H 4600 1100 50  0001 C CNN
	1    4600 1100
	1    0    0    -1  
$EndComp
$Comp
L vextreme:+5V #PWR0117
U 1 1 5D06ED8C
P 1350 1000
F 0 "#PWR0117" H 1350 850 50  0001 C CNN
F 1 "+5V" H 1350 1140 50  0000 C CNN
F 2 "" H 1350 1000 50  0001 C CNN
F 3 "" H 1350 1000 50  0001 C CNN
	1    1350 1000
	1    0    0    -1  
$EndComp
Wire Wire Line
	950  2100 1100 2100
Text GLabel 3100 6700 2    50   Input ~ 0
USB_D+
Text GLabel 3100 6400 2    50   Input ~ 0
USB_D-
Text GLabel 5250 2650 2    50   Input ~ 0
USB_D+
Text GLabel 5250 2550 2    50   Input ~ 0
USB_D-
Wire Wire Line
	4050 4750 4150 4750
Connection ~ 4150 4750
Wire Wire Line
	4150 4750 4250 4750
Connection ~ 4250 4750
Wire Wire Line
	4250 4750 4350 4750
Connection ~ 4350 4750
$Comp
L vextreme:GND #PWR0119
U 1 1 5D07FB9D
P 4700 4850
F 0 "#PWR0119" H 4700 4600 50  0001 C CNN
F 1 "GND" H 4700 4700 50  0000 C CNN
F 2 "" H 4700 4850 50  0001 C CNN
F 3 "" H 4700 4850 50  0001 C CNN
	1    4700 4850
	1    0    0    -1  
$EndComp
Wire Wire Line
	4450 4750 4700 4750
Wire Wire Line
	4700 4750 4700 4850
Connection ~ 4450 4750
Wire Wire Line
	4350 4750 4450 4750
$Comp
L vextreme:C C7
U 1 1 5D0843D4
P 3400 2000
F 0 "C7" H 3500 2000 50  0000 L CNN
F 1 "2.2u" H 3450 1850 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 3438 1850 50  0001 C CNN
F 3 "~" H 3400 2000 50  0001 C CNN
	1    3400 2000
	1    0    0    -1  
$EndComp
$Comp
L vextreme:C C6
U 1 1 5D084F4B
P 3150 2000
F 0 "C6" H 2950 2000 50  0000 L CNN
F 1 "2.2u" H 2900 1850 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 3188 1850 50  0001 C CNN
F 3 "~" H 3150 2000 50  0001 C CNN
	1    3150 2000
	1    0    0    -1  
$EndComp
Wire Wire Line
	3150 1850 3400 1850
Connection ~ 3400 1850
Wire Wire Line
	3400 1850 3550 1850
$Comp
L vextreme:GND #PWR0120
U 1 1 5D08816F
P 3150 2150
F 0 "#PWR0120" H 3150 1900 50  0001 C CNN
F 1 "GND" H 3155 1977 50  0000 C CNN
F 2 "" H 3150 2150 50  0001 C CNN
F 3 "" H 3150 2150 50  0001 C CNN
	1    3150 2150
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0121
U 1 1 5D088668
P 3400 2150
F 0 "#PWR0121" H 3400 1900 50  0001 C CNN
F 1 "GND" H 3405 1977 50  0000 C CNN
F 2 "" H 3400 2150 50  0001 C CNN
F 3 "" H 3400 2150 50  0001 C CNN
	1    3400 2150
	1    0    0    -1  
$EndComp
$Comp
L vextreme:R R3
U 1 1 5D088F54
P 2800 1800
F 0 "R3" H 2870 1846 50  0000 L CNN
F 1 "10k" H 2870 1755 50  0000 L CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" V 2730 1800 50  0001 C CNN
F 3 "~" H 2800 1800 50  0001 C CNN
	1    2800 1800
	1    0    0    -1  
$EndComp
Wire Wire Line
	2800 1650 3550 1650
$Comp
L vextreme:GND #PWR0122
U 1 1 5D08BE45
P 2800 1950
F 0 "#PWR0122" H 2800 1700 50  0001 C CNN
F 1 "GND" H 2805 1777 50  0000 C CNN
F 2 "" H 2800 1950 50  0001 C CNN
F 3 "" H 2800 1950 50  0001 C CNN
	1    2800 1950
	1    0    0    -1  
$EndComp
$Comp
L vextreme:Jumper_NO_Small JP1
U 1 1 5D08D66D
P 2800 1450
F 0 "JP1" V 2846 1402 50  0000 R CNN
F 1 "Jumper" V 2750 1400 50  0000 R CNN
F 2 "vextreme:PinHeader_1x02_P2.54mm_Vertical_Odd_Even" H 2800 1450 50  0001 C CNN
F 3 "~" H 2800 1450 50  0001 C CNN
	1    2800 1450
	0    -1   -1   0   
$EndComp
Connection ~ 2800 1650
$Comp
L vextreme:+3.3V #PWR0123
U 1 1 5D08DC37
P 2800 1250
F 0 "#PWR0123" H 2800 1100 50  0001 C CNN
F 1 "+3.3V" H 2815 1423 50  0000 C CNN
F 2 "" H 2800 1250 50  0001 C CNN
F 3 "" H 2800 1250 50  0001 C CNN
	1    2800 1250
	1    0    0    -1  
$EndComp
Text GLabel 3550 2550 0    50   Input ~ 0
OSC_IN
Text GLabel 3550 2650 0    50   Input ~ 0
OSC_OUT
$Comp
L vextreme:C C8
U 1 1 5D0921F6
P 5800 5450
F 0 "C8" V 5548 5450 50  0000 C CNN
F 1 "18pF" V 5639 5450 50  0000 C CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 5838 5300 50  0001 C CNN
F 3 "~" H 5800 5450 50  0001 C CNN
	1    5800 5450
	0    1    1    0   
$EndComp
$Comp
L vextreme:C C9
U 1 1 5D093433
P 5800 5900
F 0 "C9" V 5548 5900 50  0000 C CNN
F 1 "18pF" V 5639 5900 50  0000 C CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 5838 5750 50  0001 C CNN
F 3 "~" H 5800 5900 50  0001 C CNN
	1    5800 5900
	0    1    1    0   
$EndComp
Wire Wire Line
	5950 5900 6050 5900
Wire Wire Line
	6050 5900 6050 5850
Wire Wire Line
	5650 5450 5550 5450
Wire Wire Line
	5550 5450 5550 5900
Wire Wire Line
	5550 5900 5650 5900
Connection ~ 5550 5900
Wire Wire Line
	5550 5900 5550 6000
$Comp
L vextreme:GND #PWR0124
U 1 1 5D0A1129
P 5550 6000
F 0 "#PWR0124" H 5550 5750 50  0001 C CNN
F 1 "GND" H 5550 5850 50  0000 C CNN
F 2 "" H 5550 6000 50  0001 C CNN
F 3 "" H 5550 6000 50  0001 C CNN
	1    5550 6000
	1    0    0    -1  
$EndComp
Wire Wire Line
	5950 5450 6050 5450
Text GLabel 6350 5450 2    50   Input ~ 0
OSC_IN
Text GLabel 6350 5900 2    50   Input ~ 0
OSC_OUT
Wire Wire Line
	6050 5550 6050 5450
Connection ~ 6050 5450
Wire Wire Line
	6050 5450 6350 5450
Wire Wire Line
	6050 5900 6350 5900
Connection ~ 6050 5900
Text GLabel 6350 3650 2    60   Input ~ 0
V-OE
Text GLabel 6350 3550 2    60   Input ~ 0
V-CE
Text GLabel 4950 4550 2    60   Input ~ 0
V-CE
Text GLabel 4950 2950 2    50   Input ~ 0
CSn
$Comp
L vextreme:R R4
U 1 1 5D0E54CA
P 5250 3150
F 0 "R4" V 5150 3150 50  0000 C CNN
F 1 "220" V 5250 3150 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" V 5180 3150 50  0001 C CNN
F 3 "~" H 5250 3150 50  0001 C CNN
	1    5250 3150
	0    1    1    0   
$EndComp
$Comp
L vextreme:LED D3
U 1 1 5D0E67D7
P 5550 3150
F 0 "D3" H 5550 3050 50  0000 C CNN
F 1 "LED" H 5550 3250 50  0000 C CNN
F 2 "LED_SMD:LED_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 5550 3150 50  0001 C CNN
F 3 "~" H 5550 3150 50  0001 C CNN
	1    5550 3150
	-1   0    0    1   
$EndComp
$Comp
L vextreme:GND #PWR0125
U 1 1 5D0EA5FD
P 5700 3250
F 0 "#PWR0125" H 5700 3000 50  0001 C CNN
F 1 "GND" H 5705 3077 50  0000 C CNN
F 2 "" H 5700 3250 50  0001 C CNN
F 3 "" H 5700 3250 50  0001 C CNN
	1    5700 3250
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0126
U 1 1 5D0F5289
P 6050 3750
F 0 "#PWR0126" H 6050 3500 50  0001 C CNN
F 1 "GND" H 6050 3600 50  0000 C CNN
F 2 "" H 6050 3750 50  0001 C CNN
F 3 "" H 6050 3750 50  0001 C CNN
	1    6050 3750
	1    0    0    -1  
$EndComp
$Comp
L vextreme:+5V #PWR0127
U 1 1 5D0F55E9
P 6050 3450
F 0 "#PWR0127" H 6050 3300 50  0001 C CNN
F 1 "+5V" H 6050 3590 50  0000 C CNN
F 2 "" H 6050 3450 50  0001 C CNN
F 3 "" H 6050 3450 50  0001 C CNN
	1    6050 3450
	1    0    0    -1  
$EndComp
$Comp
L vextreme:+5V #PWR0128
U 1 1 5D0F60DC
P 3850 5900
F 0 "#PWR0128" H 3850 5750 50  0001 C CNN
F 1 "+5V" H 3850 6040 50  0000 C CNN
F 2 "" H 3850 5900 50  0001 C CNN
F 3 "" H 3850 5900 50  0001 C CNN
	1    3850 5900
	1    0    0    -1  
$EndComp
Text GLabel 4950 4250 2    50   Input ~ 0
V-HALT
Wire Wire Line
	4950 3350 5250 3350
Wire Wire Line
	5250 3350 5250 3400
$Comp
L vextreme:GND #PWR0129
U 1 1 5D00E3C7
P 5250 3400
F 0 "#PWR0129" H 5250 3150 50  0001 C CNN
F 1 "GND" H 5255 3227 50  0000 C CNN
F 2 "" H 5250 3400 50  0001 C CNN
F 3 "" H 5250 3400 50  0001 C CNN
	1    5250 3400
	1    0    0    -1  
$EndComp
Text GLabel 2350 3050 2    50   Input ~ 0
C_A0
Text GLabel 2350 3150 2    50   Input ~ 0
C_A1
Text GLabel 2350 3250 2    50   Input ~ 0
C_A2
Text GLabel 2350 3350 2    50   Input ~ 0
C_A3
Text GLabel 2350 3450 2    50   Input ~ 0
C_A4
Text GLabel 2350 3550 2    50   Input ~ 0
C_A5
Text GLabel 2350 3650 2    50   Input ~ 0
C_A6
Text GLabel 2350 3750 2    50   Input ~ 0
C_A7
Text GLabel 2350 3850 2    50   Input ~ 0
C_A8
Text GLabel 2350 3950 2    50   Input ~ 0
C_A9
Text GLabel 2350 4050 2    50   Input ~ 0
C_A10
Text GLabel 2350 4150 2    50   Input ~ 0
C_A11
Text GLabel 2350 4250 2    50   Input ~ 0
C_A12
Text GLabel 2350 4350 2    50   Input ~ 0
C_A13
Text GLabel 2350 4450 2    50   Input ~ 0
C_A14
Text GLabel 4950 3750 2    50   Input ~ 0
TXD
Text GLabel 4950 3850 2    50   Input ~ 0
RXD
Text GLabel 5650 1650 0    50   Input ~ 0
P_D5
Text GLabel 5150 1950 2    50   Input ~ 0
P_D5
Text GLabel 5700 1950 0    50   Input ~ 0
P_D2
Text GLabel 5100 1650 2    50   Input ~ 0
P_D2
Text GLabel 5100 1850 2    50   Input ~ 0
P_D4
Text GLabel 5600 1750 0    50   Input ~ 0
P_D4
Text GLabel 5200 1450 2    50   Input ~ 0
P_D0
Text GLabel 5800 2150 0    50   Input ~ 0
P_D0
Text GLabel 5150 1550 2    50   Input ~ 0
P_D1
Text GLabel 5750 2050 0    50   Input ~ 0
P_D1
Text GLabel 5050 1750 2    50   Input ~ 0
P_D3
Text GLabel 5650 1850 0    50   Input ~ 0
P_D3
Text GLabel 5200 2050 2    50   Input ~ 0
P_D6
Text GLabel 5700 1550 0    50   Input ~ 0
P_D6
Text GLabel 5250 2150 2    50   Input ~ 0
P_D7
Text GLabel 5750 1450 0    50   Input ~ 0
P_D7
$Comp
L Connector:Conn_01x03_Male J2
U 1 1 5D0ADA96
P 10850 5100
F 0 "J2" H 10900 5350 50  0000 R CNN
F 1 "Conn_01x03_Male" H 11250 4900 50  0000 R CNN
F 2 "vextreme:PinHeader_1x03_P2.54mm_Vertical_Odd_Even" H 10850 5100 50  0001 C CNN
F 3 "~" H 10850 5100 50  0001 C CNN
	1    10850 5100
	-1   0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0130
U 1 1 5D0AE42B
P 10350 5000
F 0 "#PWR0130" H 10350 4750 50  0001 C CNN
F 1 "GND" H 10350 4850 50  0000 C CNN
F 2 "" H 10350 5000 50  0001 C CNN
F 3 "" H 10350 5000 50  0001 C CNN
	1    10350 5000
	1    0    0    -1  
$EndComp
Text GLabel 10650 5200 0    50   Input ~ 0
TXD
Text GLabel 10650 5100 0    50   Input ~ 0
RXD
Text GLabel 3100 5550 2    50   Input ~ 0
USB-5V
$Comp
L vextreme:GND #PWR0118
U 1 1 5DEA6FB0
P 1150 7400
F 0 "#PWR0118" H 1150 7150 50  0001 C CNN
F 1 "GND" H 1150 7250 50  0000 C CNN
F 2 "" H 1150 7400 50  0001 C CNN
F 3 "" H 1150 7400 50  0001 C CNN
	1    1150 7400
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0131
U 1 1 5DEA771E
P 850 7400
F 0 "#PWR0131" H 850 7150 50  0001 C CNN
F 1 "GND" H 850 7250 50  0000 C CNN
F 2 "" H 850 7400 50  0001 C CNN
F 3 "" H 850 7400 50  0001 C CNN
	1    850  7400
	1    0    0    -1  
$EndComp
Wire Wire Line
	2300 6500 2300 6400
Wire Wire Line
	2200 6600 2200 6700
Wire Wire Line
	2200 6700 2550 6700
Wire Wire Line
	2850 6100 3250 6100
Wire Wire Line
	3250 6100 3250 6200
Wire Wire Line
	3250 6200 3100 6200
$Comp
L vextreme:GND #PWR0132
U 1 1 5DF1039D
P 3450 6100
F 0 "#PWR0132" H 3450 5850 50  0001 C CNN
F 1 "GND" H 3450 5950 50  0000 C CNN
F 2 "" H 3450 6100 50  0001 C CNN
F 3 "" H 3450 6100 50  0001 C CNN
	1    3450 6100
	1    0    0    -1  
$EndComp
$Comp
L vextreme:USB_C_Receptacle_USB2.0 J1
U 1 1 5D00696D
P 1150 6500
F 0 "J1" H 800 7350 50  0000 C CNN
F 1 "USB_C_Receptacle_USB2.0" H 1250 7250 50  0000 C CNN
F 2 "vextreme:USB_C_Receptacle_HRO_TYPE-C-31-M-12" H 3979 2716 50  0001 C CNN
F 3 "~" H 1300 6450 50  0001 C CNN
	1    1150 6500
	1    0    0    -1  
$EndComp
$Comp
L vextreme:R R1
U 1 1 5D076421
P 2700 6700
F 0 "R1" V 2600 6700 50  0000 C CNN
F 1 "22" V 2700 6700 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" V 2630 6700 50  0001 C CNN
F 3 "~" H 2700 6700 50  0001 C CNN
	1    2700 6700
	0    1    1    0   
$EndComp
$Comp
L vextreme:R R2
U 1 1 5D076F9A
P 2700 6400
F 0 "R2" V 2800 6400 50  0000 C CNN
F 1 "22" V 2700 6400 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" V 2630 6400 50  0001 C CNN
F 3 "~" H 2700 6400 50  0001 C CNN
	1    2700 6400
	0    -1   -1   0   
$EndComp
$Comp
L vextreme:R R5
U 1 1 5DEF494D
P 2700 6100
F 0 "R5" V 2600 6100 50  0000 C CNN
F 1 "5.1k" V 2700 6100 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" V 2630 6100 50  0001 C CNN
F 3 "~" H 2700 6100 50  0001 C CNN
	1    2700 6100
	0    1    1    0   
$EndComp
$Comp
L vextreme:R R6
U 1 1 5DEFEB29
P 2950 6200
F 0 "R6" V 2750 6200 50  0000 C CNN
F 1 "5.1k" V 2950 6200 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" V 2880 6200 50  0001 C CNN
F 3 "~" H 2950 6200 50  0001 C CNN
	1    2950 6200
	0    1    1    0   
$EndComp
Wire Wire Line
	3250 6100 3450 6100
Connection ~ 3250 6100
Text GLabel 4950 4050 2    60   Input ~ 0
V-IRQ
Text GLabel 4950 4350 2    60   Input ~ 0
SCLK
Text GLabel 4950 4450 2    60   Input ~ 0
SDAT
Text GLabel 7750 5950 0    60   Input ~ 0
SCLK
Text GLabel 7750 5850 0    60   Input ~ 0
SDAT
Wire Wire Line
	5700 3150 5700 3250
Wire Wire Line
	5100 3150 4950 3150
$Comp
L vextreme-rescue:GND-vextreme-rescue #PWR0139
U 1 1 5E22D72D
P 8100 6250
F 0 "#PWR0139" H 8100 6000 50  0001 C CNN
F 1 "GND" H 8100 6100 50  0000 C CNN
F 2 "" H 8100 6250 50  0001 C CNN
F 3 "" H 8100 6250 50  0001 C CNN
	1    8100 6250
	1    0    0    -1  
$EndComp
$Comp
L vextreme:+5V #PWR0140
U 1 1 5E22D733
P 8100 5650
F 0 "#PWR0140" H 8100 5500 50  0001 C CNN
F 1 "+5V" H 8100 5790 50  0000 C CNN
F 2 "" H 8100 5650 50  0001 C CNN
F 3 "" H 8100 5650 50  0001 C CNN
	1    8100 5650
	1    0    0    -1  
$EndComp
Wire Wire Line
	7750 5850 7800 5850
Wire Wire Line
	7750 5950 7800 5950
Wire Wire Line
	8400 5850 8450 5850
Wire Wire Line
	8400 5950 8450 5950
Wire Wire Line
	7350 750  7350 800 
Wire Wire Line
	6650 800  7350 800 
Connection ~ 5300 800 
Connection ~ 4950 800 
Wire Wire Line
	4600 800  4950 800 
Wire Wire Line
	4950 800  5300 800 
Wire Wire Line
	4450 1250 4550 1250
Wire Wire Line
	4350 800  4350 1250
Wire Wire Line
	4350 800  4600 800 
Wire Wire Line
	2850 6400 3100 6400
Wire Wire Line
	2850 6700 3100 6700
Text Notes 7950 1850 0    50   ~ 0
v0.3 Changelog\n——————————————————————\n- Fixed PCB fingers (removed soldermask ink around fingers, made them 8.8mm\n   x 1.52mm, spaced 1.2mm from board edge, and 18.0mm from 3.6mm hole)\n- Increased hole sizes to 3.7mm, 4.2mm, 5.2mm\n- Made all pads on addressable LEDs the same size\n- Remove D8 - D13, spacing remaining 4 LEDs evenly (13mm)\n- Change debugging header to 3-pin\n- Add 3-pin header to output LED signals (drive 1000’s of LEDs if you want)\n- Added SW1 for programming, and SW2 for STM32 reset\n- Changed C13 from 47uF to 10uF\n- Added cut & solder jumper to convert SW2 to a User Button\n- Added test points for main voltage rails\n- Connected Vectrex 5V directly to PA10\n- Connected V-OE to PB10, changed U3 to single OR gate, and added R7 for\n   optional way to assert U4 /OE in the future potentially.
$Comp
L vextreme:Crystal Y1
U 1 1 5D00550C
P 6050 5700
F 0 "Y1" V 5950 5900 50  0000 C CNN
F 1 "8MHz" V 6050 5950 50  0000 C CNN
F 2 "Crystal:Crystal_SMD_5032-2Pin_5.0x3.2mm_HandSoldering" H 6050 5700 50  0001 C CNN
F 3 "https://abracon.com/Resonators/abm3.pdf" H 6050 5700 50  0001 C CNN
F 4 "12pF" V 6150 5950 50  0000 C CNN "Cload"
	1    6050 5700
	0    1    1    0   
$EndComp
$Comp
L MCU_ST_STM32F4:STM32F411RCTx U2
U 1 1 5CFFE17C
P 4250 2950
F 0 "U2" H 3700 4600 50  0000 C CNN
F 1 "STM32F411RCTx" H 3650 1200 50  0000 C CNN
F 2 "Package_QFP:LQFP-64_10x10mm_P0.5mm" H 3650 1250 50  0001 R CNN
F 3 "http://www.st.com/st-web-ui/static/active/en/resource/technical/document/datasheet/DM00115249.pdf" H 4250 2950 50  0001 C CNN
	1    4250 2950
	1    0    0    -1  
$EndComp
$Comp
L vextreme:SN74LVC8T245DB U4
U 1 1 5CFEBA56
P 6500 1950
F 0 "U4" H 6950 2600 50  0000 C CNN
F 1 "SN74LVC8T245DB" H 6950 1300 50  0000 C CNN
F 2 "Package_SO:SSOP-24_5.3x8.2mm_P0.65mm" H 6500 750 50  0001 C CNN
F 3 "http://www.ti.com/lit/ds/symlink/sn74lvc8t245.pdf" H 6600 2300 50  0001 C CNN
	1    6500 1950
	1    0    0    -1  
$EndComp
$Comp
L vextreme:W25Q128 U5
U 1 1 5CFEAE1F
P 6450 4600
F 0 "U5" H 6450 4900 50  0000 C CNN
F 1 "W25Q128" H 6450 4300 50  0000 C CNN
F 2 "Package_SO:SOIC-8_5.23x5.23mm_P1.27mm" H 6750 4700 50  0001 C CNN
F 3 "https://www.pjrc.com/teensy/W25Q128FV.pdf" H 6750 4700 50  0001 C CNN
	1    6450 4600
	1    0    0    -1  
$EndComp
$Comp
L LED:APA102-2020 D4
U 1 1 5E22D727
P 8100 5950
F 0 "D4" H 7950 6200 50  0000 C CNN
F 1 "APA102-2020" H 8400 5700 50  0000 C CNN
F 2 "vextreme:LED-APA102-2020_6_REV_2" H 8150 5650 50  0001 L TNN
F 3 "http://www.led-color.com/upload/201604/APA102-2020%20SMD%20LED.pdf" H 8200 5575 50  0001 L TNN
	1    8100 5950
	1    0    0    -1  
$EndComp
$Comp
L vextreme:vectrex-edge-connector CON1
U 1 1 59F1631A
P 1800 3500
F 0 "CON1" H 1450 5350 60  0000 C CNN
F 1 "vectrex-edge-connector" V 1400 3250 60  0000 C CNN
F 2 "vextreme:vectrex-edge-connector-thinner-no-soldermask" H 1800 4200 60  0001 C CNN
F 3 "" H 1800 4200 60  0001 C CNN
	1    1800 3500
	1    0    0    -1  
$EndComp
Wire Wire Line
	4600 5900 4600 5700
$Comp
L vextreme:+3.3V #PWR0104
U 1 1 5D013933
P 4600 5700
F 0 "#PWR0104" H 4600 5550 50  0001 C CNN
F 1 "+3.3V" H 4615 5873 50  0000 C CNN
F 2 "" H 4600 5700 50  0001 C CNN
F 3 "" H 4600 5700 50  0001 C CNN
	1    4600 5700
	1    0    0    -1  
$EndComp
$Comp
L vextreme:C C13
U 1 1 5DEB0479
P 7350 6150
F 0 "C13" H 7465 6196 50  0000 L CNN
F 1 "10u" H 7465 6105 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric_Pad1.15x1.40mm_HandSolder" H 7388 6000 50  0001 C CNN
F 3 "~" H 7350 6150 50  0001 C CNN
	1    7350 6150
	1    0    0    -1  
$EndComp
$Comp
L vextreme:GND #PWR0154
U 1 1 5DEB047F
P 7350 6300
F 0 "#PWR0154" H 7350 6050 50  0001 C CNN
F 1 "GND" H 7350 6150 50  0000 C CNN
F 2 "" H 7350 6300 50  0001 C CNN
F 3 "" H 7350 6300 50  0001 C CNN
	1    7350 6300
	1    0    0    -1  
$EndComp
$Comp
L vextreme:+5V #PWR0155
U 1 1 5DEB5A91
P 7350 6000
F 0 "#PWR0155" H 7350 5850 50  0001 C CNN
F 1 "+5V" H 7350 6140 50  0000 C CNN
F 2 "" H 7350 6000 50  0001 C CNN
F 3 "" H 7350 6000 50  0001 C CNN
	1    7350 6000
	1    0    0    -1  
$EndComp
$Comp
L LED:APA102-2020 D6
U 1 1 5E241681
P 9450 5950
F 0 "D6" H 9300 6200 50  0000 C CNN
F 1 "APA102-2020" H 9750 5700 50  0000 C CNN
F 2 "vextreme:LED-APA102-2020_6_REV_2" H 9500 5650 50  0001 L TNN
F 3 "http://www.led-color.com/upload/201604/APA102-2020%20SMD%20LED.pdf" H 9550 5575 50  0001 L TNN
	1    9450 5950
	1    0    0    -1  
$EndComp
$Comp
L LED:APA102-2020 D7
U 1 1 5E24CDA4
P 10150 5950
F 0 "D7" H 10000 6200 50  0000 C CNN
F 1 "APA102-2020" H 10450 5700 50  0000 C CNN
F 2 "vextreme:LED-APA102-2020_6_REV_2" H 10200 5650 50  0001 L TNN
F 3 "http://www.led-color.com/upload/201604/APA102-2020%20SMD%20LED.pdf" H 10250 5575 50  0001 L TNN
	1    10150 5950
	1    0    0    -1  
$EndComp
$Comp
L vextreme-rescue:+5V-vextreme-rescue #PWR0152
U 1 1 5E24CDB0
P 10150 5650
F 0 "#PWR0152" H 10150 5500 50  0001 C CNN
F 1 "+5V" H 10150 5790 50  0000 C CNN
F 2 "" H 10150 5650 50  0001 C CNN
F 3 "" H 10150 5650 50  0001 C CNN
	1    10150 5650
	1    0    0    -1  
$EndComp
$Comp
L vextreme-rescue:GND-vextreme-rescue #PWR0151
U 1 1 5E24CDAA
P 10150 6250
F 0 "#PWR0151" H 10150 6000 50  0001 C CNN
F 1 "GND" H 10150 6100 50  0000 C CNN
F 2 "" H 10150 6250 50  0001 C CNN
F 3 "" H 10150 6250 50  0001 C CNN
	1    10150 6250
	1    0    0    -1  
$EndComp
$Comp
L vextreme-rescue:+5V-vextreme-rescue #PWR0148
U 1 1 5E24168D
P 9450 5650
F 0 "#PWR0148" H 9450 5500 50  0001 C CNN
F 1 "+5V" H 9450 5790 50  0000 C CNN
F 2 "" H 9450 5650 50  0001 C CNN
F 3 "" H 9450 5650 50  0001 C CNN
	1    9450 5650
	1    0    0    -1  
$EndComp
$Comp
L vextreme-rescue:GND-vextreme-rescue #PWR0147
U 1 1 5E241687
P 9450 6250
F 0 "#PWR0147" H 9450 6000 50  0001 C CNN
F 1 "GND" H 9450 6100 50  0000 C CNN
F 2 "" H 9450 6250 50  0001 C CNN
F 3 "" H 9450 6250 50  0001 C CNN
	1    9450 6250
	1    0    0    -1  
$EndComp
$Comp
L vextreme-rescue:GND-vextreme-rescue #PWR0143
U 1 1 5E237D78
P 8750 6250
F 0 "#PWR0143" H 8750 6000 50  0001 C CNN
F 1 "GND" H 8750 6100 50  0000 C CNN
F 2 "" H 8750 6250 50  0001 C CNN
F 3 "" H 8750 6250 50  0001 C CNN
	1    8750 6250
	1    0    0    -1  
$EndComp
$Comp
L vextreme-rescue:+5V-vextreme-rescue #PWR0144
U 1 1 5E237D7E
P 8750 5650
F 0 "#PWR0144" H 8750 5500 50  0001 C CNN
F 1 "+5V" H 8750 5790 50  0000 C CNN
F 2 "" H 8750 5650 50  0001 C CNN
F 3 "" H 8750 5650 50  0001 C CNN
	1    8750 5650
	1    0    0    -1  
$EndComp
$Comp
L LED:APA102-2020 D5
U 1 1 5E237D72
P 8750 5950
F 0 "D5" H 8600 6200 50  0000 C CNN
F 1 "APA102-2020" H 9050 5700 50  0000 C CNN
F 2 "vextreme:LED-APA102-2020_6_REV_2" H 8800 5650 50  0001 L TNN
F 3 "http://www.led-color.com/upload/201604/APA102-2020%20SMD%20LED.pdf" H 8850 5575 50  0001 L TNN
	1    8750 5950
	1    0    0    -1  
$EndComp
Wire Wire Line
	9050 5950 9150 5950
Wire Wire Line
	9050 5850 9150 5850
Wire Wire Line
	9750 5950 9850 5950
Wire Wire Line
	9750 5850 9850 5850
Wire Wire Line
	10350 5000 10650 5000
$Comp
L Connector:Conn_01x03_Male J3
U 1 1 5EEA03A9
P 10850 5850
F 0 "J3" H 10950 6150 50  0000 R CNN
F 1 "Conn_01x03_Male" H 11250 5650 50  0000 R CNN
F 2 "vextreme:PinHeader_1x03_P2.54mm_Vertical_Odd_Even" H 10850 5850 50  0001 C CNN
F 3 "~" H 10850 5850 50  0001 C CNN
	1    10850 5850
	-1   0    0    -1  
$EndComp
Wire Wire Line
	10650 5950 10450 5950
Wire Wire Line
	10450 5850 10650 5850
$Comp
L vextreme-rescue:GND-vextreme-rescue #PWR0133
U 1 1 5EEAFF3A
P 10500 5550
F 0 "#PWR0133" H 10500 5300 50  0001 C CNN
F 1 "GND" H 10500 5400 50  0000 C CNN
F 2 "" H 10500 5550 50  0001 C CNN
F 3 "" H 10500 5550 50  0001 C CNN
	1    10500 5550
	1    0    0    -1  
$EndComp
Wire Wire Line
	10650 5550 10500 5550
Wire Wire Line
	10650 5550 10650 5750
Wire Wire Line
	2650 1650 2800 1650
Wire Wire Line
	2650 1250 2800 1250
Wire Wire Line
	2800 1250 2800 1350
Connection ~ 2800 1250
$Comp
L Switch:SW_Push SW2
U 1 1 5EEA4DC7
P 5600 7200
F 0 "SW2" H 5600 7500 50  0000 C CNN
F 1 "TS-1002S-12026C" H 5500 7400 50  0000 C CNN
F 2 "vextreme:SW_Push_1P1T_NO_6x6mm_H12.0mm" H 5600 7400 50  0001 C CNN
F 3 "~" H 5600 7400 50  0001 C CNN
	1    5600 7200
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0134
U 1 1 5EEA68C6
P 5400 7200
F 0 "#PWR0134" H 5400 6950 50  0001 C CNN
F 1 "GND" H 5350 7250 50  0000 C CNN
F 2 "" H 5400 7200 50  0001 C CNN
F 3 "" H 5400 7200 50  0001 C CNN
	1    5400 7200
	1    0    0    -1  
$EndComp
Connection ~ 1850 1450
$Comp
L Device:R R7
U 1 1 5EEE1496
P 5550 3950
F 0 "R7" V 5460 3900 50  0000 C CNN
F 1 "0" V 5550 3950 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.15x1.40mm_HandSolder" V 5480 3950 50  0001 C CNN
F 3 "~" H 5550 3950 50  0001 C CNN
F 4 "DNP" V 5640 3930 50  0000 C CNN "Field4"
	1    5550 3950
	0    1    1    0   
$EndComp
Wire Wire Line
	4950 3950 5400 3950
Wire Wire Line
	5700 3950 5800 3950
Wire Wire Line
	6050 3700 6050 3750
Wire Wire Line
	6050 3450 6050 3500
Text GLabel 4950 4150 2    60   Input ~ 0
V-OE
Text Notes 5290 4180 0    50   ~ 0
/E
Text Notes 5270 4590 0    50   ~ 0
A15
Text GLabel 4950 2450 2    50   Input ~ 0
VEC-5V
Text GLabel 1950 1050 2    50   Input ~ 0
VEC-5V
Wire Wire Line
	3100 5550 2400 5550
Wire Wire Line
	2400 5550 2400 5900
Wire Wire Line
	2400 5900 3350 5900
$Comp
L Connector:TestPoint TP1
U 1 1 5EFBE655
P 1950 900
F 0 "TP1" V 1850 1150 50  0000 C CNN
F 1 "V5V" V 1950 1150 50  0000 C CNN
F 2 "TestPoint:TestPoint_Pad_D1.5mm" H 2150 900 50  0001 C CNN
F 3 "~" H 2150 900 50  0001 C CNN
	1    1950 900 
	0    1    1    0   
$EndComp
$Comp
L Connector:TestPoint TP2
U 1 1 5EFD3FB2
P 1250 1050
F 0 "TP2" V 1350 1350 50  0000 C CNN
F 1 "5V" V 1250 1350 50  0000 C CNN
F 2 "TestPoint:TestPoint_Pad_D1.5mm" H 1450 1050 50  0001 C CNN
F 3 "~" H 1450 1050 50  0001 C CNN
	1    1250 1050
	0    -1   -1   0   
$EndComp
Wire Wire Line
	1350 1050 1350 1000
Wire Wire Line
	1350 1050 1250 1050
Connection ~ 1350 1050
$Comp
L Connector:TestPoint TP3
U 1 1 5EFE3E30
P 1600 5200
F 0 "TP3" V 1700 5450 50  0000 C CNN
F 1 "GND" V 1600 5450 50  0000 C CNN
F 2 "TestPoint:TestPoint_Pad_D1.5mm" H 1800 5200 50  0001 C CNN
F 3 "~" H 1800 5200 50  0001 C CNN
	1    1600 5200
	0    -1   -1   0   
$EndComp
Wire Wire Line
	1600 5200 1700 5200
$Comp
L Connector:TestPoint TP4
U 1 1 5EFF146E
P 2400 5550
F 0 "TP4" V 2600 5650 50  0000 C CNN
F 1 "U5V" V 2500 5650 50  0000 C CNN
F 2 "TestPoint:TestPoint_Pad_D1.5mm" H 2600 5550 50  0001 C CNN
F 3 "~" H 2600 5550 50  0001 C CNN
	1    2400 5550
	0    -1   -1   0   
$EndComp
Connection ~ 2400 5550
$Comp
L Connector:TestPoint TP5
U 1 1 5EFF3A70
P 4650 5900
F 0 "TP5" V 4450 6050 50  0000 C CNN
F 1 "3V3" V 4550 6050 50  0000 C CNN
F 2 "TestPoint:TestPoint_Pad_D1.5mm" H 4850 5900 50  0001 C CNN
F 3 "~" H 4850 5900 50  0001 C CNN
	1    4650 5900
	0    1    1    0   
$EndComp
Wire Wire Line
	4650 5900 4600 5900
$Comp
L Graphic:Logo_Open_Hardware_Small #LOGO1
U 1 1 5EE7732D
P 7300 6850
F 0 "#LOGO1" H 7300 7125 50  0001 C CNN
F 1 "Logo_Open_Hardware_Small" H 7300 6625 50  0001 C CNN
F 2 "" H 7300 6850 50  0001 C CNN
F 3 "~" H 7300 6850 50  0001 C CNN
	1    7300 6850
	1    0    0    -1  
$EndComp
Connection ~ 5800 3600
Wire Wire Line
	5800 2450 5800 3600
Wire Wire Line
	5800 3950 5800 3600
$Comp
L 74xGxx:74LVC1G32 U3
U 1 1 5EED04ED
P 6050 3600
F 0 "U3" H 5850 3850 50  0000 C CNN
F 1 "74LVC1G32" H 5700 3750 50  0000 C CNN
F 2 "vextreme:SOT-23-5_HandSoldering" H 6050 3600 50  0001 C CNN
F 3 "http://www.ti.com/lit/sg/scyt129e/scyt129e.pdf" H 6050 3600 50  0001 C CNN
	1    6050 3600
	-1   0    0    -1  
$EndComp
$Comp
L Connector:TestPoint SP1
U 1 1 5EF3F178
P 5200 2250
F 0 "SP1" V 5200 2550 50  0001 C CNN
F 1 "PA8" V 5200 2500 50  0000 C CNN
F 2 "vextreme:SparePin_1x01_P2.54mm_Vertical" H 5400 2250 50  0001 C CNN
F 3 "~" H 5400 2250 50  0001 C CNN
	1    5200 2250
	0    1    1    0   
$EndComp
$Comp
L Connector:TestPoint SP2
U 1 1 5EF659ED
P 5200 2750
F 0 "SP2" V 5300 3200 50  0001 C CNN
F 1 "PA13" V 5200 3000 50  0000 C CNN
F 2 "vextreme:SparePin_1x01_P2.54mm_Vertical" H 5400 2750 50  0001 C CNN
F 3 "~" H 5400 2750 50  0001 C CNN
	1    5200 2750
	0    1    1    0   
$EndComp
$Comp
L Connector:TestPoint SP3
U 1 1 5EF70257
P 5200 2850
F 0 "SP3" V 5300 3300 50  0001 C CNN
F 1 "PA14" V 5200 3100 50  0000 C CNN
F 2 "vextreme:SparePin_1x01_P2.54mm_Vertical" H 5400 2850 50  0001 C CNN
F 3 "~" H 5400 2850 50  0001 C CNN
	1    5200 2850
	0    1    1    0   
$EndComp
$Comp
L Connector:TestPoint SP4
U 1 1 5EF70863
P 3550 2850
F 0 "SP4" V 3650 3300 50  0001 C CNN
F 1 "PD2" V 3550 3100 50  0000 C CNN
F 2 "vextreme:SparePin_1x01_P2.54mm_Vertical" H 3750 2850 50  0001 C CNN
F 3 "~" H 3750 2850 50  0001 C CNN
	1    3550 2850
	0    -1   -1   0   
$EndComp
Connection ~ 2400 5900
Connection ~ 2200 6700
Connection ~ 2300 6400
Wire Wire Line
	1750 6700 2200 6700
Wire Wire Line
	1750 6500 2300 6500
Wire Wire Line
	2000 6800 2000 5900
Connection ~ 2000 6800
Wire Wire Line
	1900 6800 2000 6800
Wire Wire Line
	1900 6850 1900 6800
$Comp
L vextreme-rescue:GND-vextreme-rescue #PWR01
U 1 1 5EE9BB2A
P 2100 7350
F 0 "#PWR01" H 2100 7100 50  0001 C CNN
F 1 "GND" H 2100 7200 50  0000 C CNN
F 2 "" H 2100 7350 50  0001 C CNN
F 3 "" H 2100 7350 50  0001 C CNN
	1    2100 7350
	1    0    0    -1  
$EndComp
Wire Wire Line
	2000 6850 2000 6800
$Comp
L vextreme:TPD4E02B04DQA_passthrough U6
U 1 1 5EE6BB32
P 2100 7150
F 0 "U6" H 2388 7246 50  0000 L CNN
F 1 "TPD4E02B04DQA" H 2388 7155 50  0000 L CNN
F 2 "vextreme:USON-10_2.5x1.0mm_P0.5mm_passthrough" H 2400 7150 50  0001 L CNN
F 3 "http://www.ti.com/lit/ds/symlink/tpd4e02b04.pdf" H 2225 7475 50  0001 C CNN
	1    2100 7150
	1    0    0    -1  
$EndComp
Wire Wire Line
	1750 6200 2800 6200
Wire Wire Line
	1750 6100 2550 6100
Wire Wire Line
	1750 6600 2200 6600
Wire Wire Line
	1750 6400 2300 6400
Wire Wire Line
	1750 5900 2000 5900
Connection ~ 2000 5900
Wire Wire Line
	2000 5900 2400 5900
Wire Wire Line
	2200 6850 2200 6700
Wire Wire Line
	2300 6850 2300 6500
Wire Wire Line
	2300 6400 2550 6400
Connection ~ 2300 6500
$Comp
L Jumper:Jumper_3_Bridged12 JP2
U 1 1 5F1FA3BC
P 5950 7200
F 0 "JP2" V 5900 7350 50  0000 C CNN
F 1 "Jumper_3_Bridged12" V 6000 7650 50  0001 C CNN
F 2 "vextreme:SolderJumper-3_P1.3mm_Bridged12_RoundedPad1.0x1.5mm_NumberLabels" H 5950 7200 50  0001 C CNN
F 3 "~" H 5950 7200 50  0001 C CNN
	1    5950 7200
	0    1    1    0   
$EndComp
Text GLabel 6100 7450 2    50   Input ~ 0
PA8
Wire Wire Line
	6100 7450 5950 7450
Text GLabel 3550 1450 0    50   Input ~ 0
NRST
Text GLabel 6050 6950 2    50   Input ~ 0
NRST
Wire Wire Line
	5950 6950 6050 6950
Text Notes 5150 6800 0    50   ~ 0
JP2 normally connected (1-2) for SW2 to\nact as hard reset for STM32.  If (1-2) cut\nand (2-3) soldered SW2 will be connected\nto pull PA8 low (should enable internal\npull-up) to use.
Wire Notes Line style solid
	5100 6350 6850 6350
Wire Notes Line style solid
	6850 6350 6850 7750
Wire Notes Line style solid
	6850 7750 5100 7750
Wire Notes Line style solid
	5100 7750 5100 6350
Wire Wire Line
	5550 4250 5550 4650
Connection ~ 5550 4650
Wire Wire Line
	5550 4650 5550 4750
Text Notes 5700 6250 0    50   ~ 0
CL = (C8 * C9)/(C8 + C9) + Cstray\n(324pF/36pF) + 4pF = 13pF\n(close to 12pF Y1 CL requirement)\n
Text Notes 7950 3000 0    50   ~ 0
v0.4 Changelog\n——————————————————————\n- Increased length of SW2 to 12mm (TS-1002S-12026C)\n- Changed SW1 to be the same as SW2\n- Replaced J2 & J3 with a single 2x3 header that presses into place and holds\n   itself for soldering. Can use two separate 1x3 headers if desired.\n- Replaced JP1 with a 1x2 header that presses into place for soldering.\n- Added a micro 10-pin JTAG debugging header that doubles as USB Serial\n   adapter as well.\n- Added a solderable jumper on V-NMI to be connected to one of PA8, which \n   also can be optionally connected to SW2.\n- Added a diode for reverse polarity (cart upside down) on VEC_5V detect line.\n- Added a “this side up” label.
$Comp
L Switch:SW_Push SW1
U 1 1 5F67E69E
P 2650 1450
F 0 "SW1" V 2900 1600 50  0000 C CNN
F 1 "TS-1002S-12026C" H 2450 1650 50  0000 C CNN
F 2 "vextreme:SW_Push_1P1T_NO_6x6mm_H12.0mm" H 2650 1650 50  0001 C CNN
F 3 "~" H 2650 1650 50  0001 C CNN
	1    2650 1450
	0    -1   -1   0   
$EndComp
Wire Wire Line
	2800 1550 2800 1650
$Comp
L Connector_Generic:Conn_02x05_Odd_Even J4
U 1 1 5F76D526
P 8200 4650
F 0 "J4" H 8200 5050 50  0000 C CNN
F 1 "FTSH-105-01-F-DV-K" H 8600 4950 50  0000 C CNN
F 2 "vextreme:CONN5X2_1.27" H 8200 4650 50  0001 C CNN
F 3 "~" H 8200 4650 50  0001 C CNN
	1    8200 4650
	1    0    0    -1  
$EndComp
Text GLabel 8500 4450 2    50   Input ~ 0
SWDIO
Text GLabel 8500 4550 2    50   Input ~ 0
SWCLK
Text GLabel 8000 4550 0    50   Input ~ 0
GND
Text GLabel 8000 4650 0    50   Input ~ 0
GND
Text GLabel 8000 4850 0    50   Input ~ 0
GND
$Comp
L Jumper:SolderJumper_2_Open JP3
U 1 1 5F77CF19
P 7650 4450
F 0 "JP3" H 7650 4650 50  0000 C CNN
F 1 "+3.3V Jumper" H 7700 4550 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm" H 7650 4450 50  0001 C CNN
F 3 "~" H 7650 4450 50  0001 C CNN
	1    7650 4450
	1    0    0    -1  
$EndComp
Wire Wire Line
	7500 4450 7100 4450
Wire Wire Line
	7800 4450 8000 4450
Text GLabel 8500 4850 2    50   Input ~ 0
NRST
Wire Wire Line
	5200 2750 4950 2750
Wire Wire Line
	4950 2850 5200 2850
Text Label 5000 2750 0    50   ~ 0
SWDIO
Text Label 5000 2850 0    50   ~ 0
SWCLK
Wire Wire Line
	4950 2550 5250 2550
Wire Wire Line
	5250 2650 4950 2650
NoConn ~ 8500 4750
NoConn ~ 8000 4750
NoConn ~ 8500 4650
Text GLabel 8000 5150 0    50   Input ~ 0
PA14
Text GLabel 8000 5050 0    50   Input ~ 0
PA13
Text GLabel 8500 5150 2    50   Input ~ 0
SWCLK
Text GLabel 8500 5050 2    50   Input ~ 0
SWDIO
Wire Wire Line
	4950 2250 5200 2250
Wire Wire Line
	5250 2150 4950 2150
Wire Wire Line
	5200 2050 4950 2050
Wire Wire Line
	5150 1950 4950 1950
Wire Wire Line
	5100 1850 4950 1850
Wire Wire Line
	5050 1750 4950 1750
Wire Wire Line
	5100 1650 4950 1650
Text Label 5000 2250 0    50   ~ 0
NMI
Wire Wire Line
	5200 1450 4950 1450
Wire Wire Line
	5150 1550 4950 1550
Wire Wire Line
	5800 1550 5700 1550
Wire Wire Line
	5650 1650 5800 1650
Wire Wire Line
	5800 1750 5600 1750
Wire Wire Line
	5650 1850 5800 1850
Wire Wire Line
	5800 1950 5700 1950
Wire Wire Line
	5750 1450 5800 1450
Wire Wire Line
	5750 2050 5800 2050
$Comp
L Jumper:SolderJumper_2_Open JP4
U 1 1 5FBFA839
P 5950 7650
F 0 "JP4" H 5550 7750 50  0000 C CNN
F 1 "V-NMI Jumper" H 6000 7750 50  0000 C CNN
F 2 "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm" H 5950 7650 50  0001 C CNN
F 3 "~" H 5950 7650 50  0001 C CNN
	1    5950 7650
	1    0    0    -1  
$EndComp
Text GLabel 5800 7650 0    60   Input ~ 0
V-NMI
Text GLabel 6100 7650 2    50   Input ~ 0
NMI
$Comp
L Diode:MBR0520 D8
U 1 1 5FC8CE3C
P 1850 1200
F 0 "D8" V 1804 1279 50  0000 L CNN
F 1 "MBR0520" V 1895 1279 50  0000 L CNN
F 2 "Diode_SMD:D_SOD-123" H 1850 1025 50  0001 C CNN
F 3 "http://www.mccsemi.com/up_pdf/MBR0520~MBR0580(SOD123).pdf" H 1850 1200 50  0001 C CNN
	1    1850 1200
	0    1    1    0   
$EndComp
Wire Wire Line
	1350 1450 1350 1350
Wire Wire Line
	1350 1450 1750 1450
Wire Wire Line
	1850 1450 1850 1350
Wire Wire Line
	1950 900  1850 900 
Wire Wire Line
	1850 900  1850 1050
Wire Wire Line
	1950 1050 1850 1050
Connection ~ 1850 1050
Wire Wire Line
	8000 5050 8500 5050
Wire Wire Line
	8000 5150 8500 5150
$EndSCHEMATC
