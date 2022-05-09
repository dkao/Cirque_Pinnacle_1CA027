// Copyright (c) 2018 Cirque Corp. Restrictions apply. See: www.cirque.com/sw-license

Designed for use with SPI TM0XX0XX trackpads fitted with Cirque's curved overlay.
The config parameters used in this sample are optimized for use with REXT = 976k.

***Modified from SPI_CurvedOverlay to prototype inertial cursor and outer ring scroll.***

Example output from serial terminal:

    Pinnacle Initialized...

    Setting ADC gain...
    ADC gain set to:        40 (X/2)

    Setting xAxis.WideZMin...
    Current value:  6
    New value:      4

    Setting yAxis.WideZMin...
    Current value:  5
    New value:      3

    X       Y       Z       Btn     Dist    dX      dY      Speed   Status
    -14.00  -44.00  16      0       46.17   0.00    0.00    0.00    valid
    -14.00  -44.00  17      0       46.17   0.00    0.00    0.00    valid
    -14.00  -44.00  20      0       46.17   0.00    0.00    0.00    valid
    -14.00  -44.00  20      0       46.17   0.00    0.00    0.00    valid
    -14.00  -44.00  21      0       46.17   0.00    0.00    0.00    valid
    -13.00  -42.00  20      0       43.97   1.00    2.00    2.24    valid
    -12.00  -40.00  21      0       41.76   1.00    2.00    2.24    valid
    -9.00   -37.00  21      0       38.08   3.00    3.00    4.24    valid
    -6.00   -34.00  20      0       34.53   3.00    3.00    4.24    valid
    -3.00   -30.00  19      0       30.15   3.00    4.00    5.00    valid
    0.00    -26.00  19      0       26.00   3.00    4.00    5.00    valid
    3.00    -22.00  15      0       22.20   3.00    4.00    5.00    hovering
    8.00    -17.00  12      0       18.79   5.00    5.00    7.07    hovering
    12.00   -9.00   7       0       15.00   4.00    8.00    8.94    hovering
    19.00   0.00    5       0       19.00   7.00    9.00    11.40   hovering
    0.00    0.00    0       0       19.00   7.00    9.00    11.40   liftoff
    6.88    8.84    0       0       11.20   8.00    6.00    10.00   glide
    0.00    0.00    0       0       19.00   7.00    9.00    11.40   liftoff
    13.51   17.37   0       0       22.00   9.00    7.00    11.40   glide
    0.00    0.00    0       0       19.00   7.00    9.00    11.40   liftoff
    19.89   25.58   0       0       32.41   8.00    6.00    10.00   glide
    0.00    0.00    0       0       19.00   7.00    9.00    11.40   liftoff
    26.04   33.47   0       0       42.41   8.00    7.00    10.63   glide
    0.00    0.00    0       0       19.00   7.00    9.00    11.40   liftoff
    31.93   41.05   0       0       52.01   8.00    5.00    9.43    glide
    37.58   48.32   0       0       61.21   7.00    6.00    9.22    glide
    42.98   55.26   0       0       70.01   7.00    5.00    8.60    glide
    48.14   61.90   0       0       78.41   6.00    6.00    8.49    glide
    53.05   68.21   0       0       86.42   7.00    5.00    8.60    glide
    57.72   74.21   0       0       94.02   6.00    4.00    7.21    glide
    62.14   79.90   0       0       101.22  5.00    5.00    7.07    glide
    66.32   85.27   0       0       108.02  6.00    4.00    7.21    glide
    70.25   90.32   0       0       114.42  5.00    4.00    6.40    glide
    73.93   95.06   0       0       120.42  5.00    3.00    5.83    glide
    77.37   99.48   0       0       126.03  4.00    4.00    5.66    glide
    80.57   103.59  0       0       131.23  4.00    3.00    5.00    glide
    83.51   107.38  0       0       136.03  4.00    3.00    5.00    glide
    86.22   110.85  0       0       140.43  3.00    3.00    4.24    glide
    88.67   114.01  0       0       144.43  4.00    2.00    4.47    glide
    90.88   116.85  0       0       148.04  2.00    2.00    2.83    glide
    92.85   119.38  0       0       151.24  3.00    2.00    3.61    glide
    94.57   121.59  0       0       154.04  2.00    2.00    2.83    glide
    96.05   123.49  0       0       156.44  2.00    2.00    2.83    glide
    97.27   125.07  0       0       158.44  2.00    1.00    2.24    glide
    98.26   126.33  0       0       160.04  1.00    1.00    1.41    glide
