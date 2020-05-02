/*************************************************
 * Public Constants
 *************************************************/

#ifndef MAX_POWER_LEVEL
#define MAX_POWER_LEVEL 20.5
#endif

typedef struct {
  int channelNumber;
  int frequency;
} WLANChannel;

const WLANChannel WiFiChannels[14] = {
  {0, 0},
  {1, 2412},
  {2, 2417},
  {3, 2422},
  {4, 2427},
  {5, 2432},
  {6, 2437},
  {7, 2442},
  {8, 2447},
  {9, 2452},
  {10, 2457},
  {11, 2462},
  {12, 2467},
  {13, 2472}
};

const int dBmPercentage[101] = {
100, // 0
100, // 1
100, // 2
100, // 3
100, // 4
100, // 5
100, // 6
100, // 7
100, // 8
100, // 9
100, // 10
100, // 11
100, // 12
100, // 13
100, // 14
100, // 15
100, // 16
100, // 17
100, // 18
100, // 19
100, // 20
99, // 21
99, // 22
99, // 23
98, // 24
98, // 25
98, // 26
97, // 27
97, // 28
96, // 29
96, // 30
95, // 31
95, // 32
94, // 33
93, // 34
93, // 35
92, // 36
91, // 37
90, // 38
90, // 39
89, // 40
88, // 41
87, // 42
86, // 43
85, // 44
84, // 45
83, // 46
82, // 47
81, // 48
80, // 49
79, // 50
78, // 51
76, // 52
75, // 53
74, // 54
73, // 55
71, // 56
70, // 57
69, // 58
67, // 59
66, // 60
64, // 61
63, // 62
61, // 63
60, // 64
58, // 65
56, // 66
55, // 67
53, // 68
51, // 69
50, // 70
48, // 71
46, // 72
44, // 73
42, // 74
40, // 75
38, // 76
36, // 77
34, // 78
32, // 79
30, // 80
28, // 81
26, // 82
24, // 83
22, // 84
20, // 85
17, // 86
15, // 87
13, // 88
10, // 89
8, // 90
6, // 91
3, // 92
1, // 93
1, // 94
1, // 95
1, // 96
1, // 97
1, // 98
1, // 99
1 // 100
};
