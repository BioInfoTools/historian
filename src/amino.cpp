#include "amino.h"
#include "jsonutil.h"

RateModel defaultAminoModel() {
  RateModel m;
  ParsedJson pj (defaultAminoModelText);
  m.read (pj.value);
  return m;
}

const char* defaultAminoModelText =
"{\n"
"    \"alphabet\": \"arndcqeghilkmfpstwyv\",\n"
"    \"subrate\" : {\n"
"	\"a\": { \"r\": 0.0137389, \"n\": 0.032145, \"d\": 0.0290394, \"c\": 0.0120395, \"q\": 0.0173705, \"e\": 0.11368, \"g\": 0.233484, \"h\": 0.00819576, \"i\": 0.0104437, \"l\": 0.0382806, \"k\": 0.013646, \"m\": 0.010476, \"f\": 0.00579197, \"p\": 0.156722, \"s\": 0.230172, \"t\": 0.182034, \"w\": 0.000225972, \"y\": 0.0109928, \"v\": 0.130478 },\n"
"	\"r\": { \"a\": 0.0354062, \"d\": 0.00293112, \"c\": 0.00566818, \"q\": 0.108957, \"e\": 0.00321786, \"g\": 0.00236413, \"h\": 0.0941947, \"i\": 0.0209864, \"l\": 0.00611536, \"k\": 0.320363, \"m\": 0.019884, \"f\": 0.00410899, \"p\": 0.0639268, \"s\": 0.104307, \"t\": 0.00539579, \"w\": 0.0179838, \"y\": 0.0025521, \"v\": 0.0135736 },\n"
"	\"n\": { \"a\": 0.0731959, \"d\": 0.545267, \"c\": 0.0016541, \"q\": 0.00610871, \"e\": 0.0133541, \"g\": 0.0849646, \"h\": 0.208984, \"i\": 0.0169964, \"l\": 0.0287232, \"k\": 0.273209, \"f\": 0.00411821, \"p\": 0.0271662, \"s\": 0.314662, \"t\": 0.115505, \"w\": 0.00161367, \"y\": 0.0343259, \"v\": 0.0212746 },\n"
"	\"d\": { \"a\": 0.068963, \"r\": 0.00270107, \"n\": 0.568675, \"c\": 0.00217445, \"q\": 0.0192285, \"e\": 0.953904, \"g\": 0.0934063, \"h\": 0.00694097, \"i\": 0.00592246, \"l\": 0.00284149, \"k\": 0.0497658, \"m\": 0.00166883, \"f\": 0.00124454, \"p\": 0.0124936, \"s\": 0.0373232, \"t\": 0.0253135, \"v\": 0.00819178 },\n"
"	\"c\": { \"a\": 0.043456, \"r\": 0.00793886, \"n\": 0.00262197, \"d\": 0.00330493, \"g\": 0.0110151, \"h\": 0.0125032, \"i\": 0.0200569, \"l\": 1.50702, \"m\": 0.000353939, \"p\": 0.0147209, \"s\": 0.107316, \"t\": 0.00611114, \"w\": 6.57388, \"y\": 0.038097, \"v\": 0.0314731 },\n"
"	\"q\": { \"a\": 0.0497271, \"r\": 0.121035, \"n\": 0.00767994, \"d\": 0.0231792, \"e\": 0.348695, \"g\": 0.0319475, \"h\": 0.278544, \"i\": 0.00137544, \"l\": 0.0899005, \"k\": 0.154502, \"m\": 0.0122557, \"p\": 0.0705534, \"s\": 0.0427736, \"t\": 0.0516448, \"v\": 0.031027 },\n"
"	\"e\": { \"a\": 0.23455, \"r\": 0.00257628, \"n\": 0.0121002, \"d\": 0.82876, \"q\": 0.251314, \"g\": 0.0786291, \"h\": 0.0180443, \"i\": 0.0249968, \"l\": 0.0079022, \"k\": 0.0855621, \"m\": 0.00238415, \"p\": 0.0303577, \"s\": 0.0849887, \"t\": 0.0179829, \"w\": 5.20212, \"y\": 0.00978899, \"v\": 0.023213 },\n"
"	\"g\": { \"a\": 0.313531, \"r\": 0.00123188, \"n\": 0.0501058, \"d\": 0.0528166, \"c\": 0.00409798, \"q\": 0.0149857, \"e\": 0.0511745, \"h\": 0.00540573, \"i\": 0.00245469, \"l\": 0.00559022, \"k\": 0.0330404, \"m\": 0.00495194, \"f\": 0.00635179, \"p\": 0.0261705, \"s\": 0.151741, \"t\": 0.0131432, \"y\": 0.000525532, \"v\": 0.0226539 },\n"
"	\"h\": { \"a\": 0.0256053, \"r\": 0.114193, \"n\": 0.286734, \"d\": 0.00913131, \"c\": 0.0108223, \"q\": 0.303986, \"e\": 0.0273231, \"g\": 0.0125768, \"i\": 0.00618497, \"l\": 0.0428964, \"k\": 0.00617126, \"m\": 0.00157941, \"f\": 0.0222249, \"p\": 0.0660071, \"s\": 0.0300311, \"t\": 0.0107019, \"w\": 0.00187191, \"y\": 0.0436021, \"v\": 0.0336793 },\n"
"	\"i\": { \"a\": 0.033318, \"r\": 0.0259798, \"n\": 0.0238126, \"d\": 0.00795606, \"c\": 0.0177275, \"q\": 0.0015328, \"e\": 0.0386506, \"g\": 0.00583173, \"h\": 0.0063157, \"l\": 0.276859, \"k\": 0.0419729, \"m\": 0.0460178, \"f\": 0.0941424, \"p\": 0.0132364, \"s\": 0.00744573, \"t\": 0.154455, \"w\": 0.000143525, \"y\": 0.0118211, \"v\": 0.555768 },\n"
"	\"l\": { \"a\": 0.0321288, \"r\": 0.00199165, \"n\": 0.0105871, \"d\": 0.00100423, \"c\": 3.50424, \"q\": 0.0263571, \"e\": 0.00321449, \"g\": 0.00349399, \"h\": 0.0115238, \"i\": 0.0728367, \"k\": 0.0135149, \"m\": 0.0777009, \"f\": 0.0509868, \"p\": 0.0120491, \"s\": 0.0144089, \"t\": 0.0145428, \"w\": 0.00278835, \"y\": 0.00886752, \"v\": 0.0726127 },\n"
"	\"k\": { \"a\": 0.0201458, \"r\": 0.183525, \"n\": 0.177134, \"d\": 0.0309373, \"q\": 0.0796767, \"e\": 0.0612221, \"g\": 0.0363247, \"h\": 0.00291616, \"i\": 0.0194233, \"l\": 0.0237726, \"m\": 0.0399115, \"f\": 0.00150391, \"p\": 0.0198995, \"s\": 0.0851466, \"t\": 0.0804877, \"y\": 0.00314481, \"v\": 0.0105565 },\n"
"	\"m\": { \"a\": 0.0561118, \"r\": 0.0413271, \"d\": 0.00376392, \"c\": 0.000525225, \"q\": 0.0229306, \"e\": 0.00618926, \"g\": 0.019752, \"h\": 0.00270776, \"i\": 0.0772608, \"l\": 0.495869, \"k\": 0.144803, \"f\": 0.0378644, \"p\": 0.00768306, \"s\": 0.0147427, \"t\": 0.0674563, \"w\": 4.36172, \"y\": 7.25993, \"v\": 0.156444 },\n"
"	\"f\": { \"a\": 0.0116814, \"r\": 0.00321571, \"n\": 0.00364757, \"d\": 0.00105694, \"g\": 0.00953984, \"h\": 0.0143472, \"i\": 0.0595153, \"l\": 0.122521, \"k\": 0.00205452, \"m\": 0.0142574, \"p\": 0.00519238, \"s\": 0.0256277, \"t\": 0.00336955, \"w\": 0.00615415, \"y\": 0.20086, \"v\": 0.0129986 },\n"
"	\"p\": { \"a\": 0.218105, \"r\": 0.0345217, \"n\": 0.0166032, \"d\": 0.00732138, \"c\": 0.00567581, \"q\": 0.0342983, \"e\": 0.0204763, \"g\": 0.0271222, \"h\": 0.0294026, \"i\": 0.00577406, \"l\": 0.019979, \"k\": 0.0187585, \"m\": 0.00199624, \"f\": 0.00358289, \"s\": 0.102331, \"t\": 0.0371096, \"w\": 0.000122152, \"v\": 0.012666 },\n"
"	\"s\": { \"a\": 0.317915, \"r\": 0.0559046, \"n\": 0.190866, \"d\": 0.0217075, \"c\": 0.0410662, \"q\": 0.0206373, \"e\": 0.0568941, \"g\": 0.156077, \"h\": 0.0132767, \"i\": 0.00322361, \"l\": 0.0237123, \"k\": 0.0796612, \"m\": 0.0038017, \"f\": 0.0175509, \"p\": 0.101561, \"t\": 0.364279, \"w\": 0.00466144, \"y\": 0.00835936, \"v\": 0.0286679 },\n"
"	\"t\": { \"a\": 0.290831, \"r\": 0.00334515, \"n\": 0.0810426, \"d\": 0.0170299, \"c\": 0.00270501, \"q\": 0.0288226, \"e\": 0.0139249, \"g\": 0.0156375, \"h\": 0.00547277, \"i\": 0.0773507, \"l\": 0.0276833, \"k\": 0.0871038, \"m\": 0.0201211, \"f\": 0.00266926, \"p\": 0.0426028, \"s\": 0.421369, \"y\": 0.0100259, \"v\": 0.112892 },\n"
"	\"w\": { \"a\": 0.00480552, \"r\": 0.148402, \"n\": 0.0150705, \"c\": 0.000387317, \"e\": 0.000536183, \"h\": 0.0127417, \"i\": 0.00095673, \"l\": 0.0706506, \"m\": 0.000173175, \"f\": 0.0648911, \"p\": 0.0018666, \"s\": 0.0717707, \"y\": 0.0322862 },\n"
"	\"y\": { \"a\": 0.0245932, \"r\": 0.00221553, \"n\": 0.0337252, \"c\": 0.0236133, \"e\": 0.0106143, \"g\": 0.000875552, \"h\": 0.0312228, \"i\": 0.00828968, \"l\": 0.023637, \"k\": 0.00476564, \"m\": 3.03236, \"f\": 0.222809, \"s\": 0.01354, \"t\": 0.0140393, \"w\": 0.00339655, \"v\": 0.0126108 },\n"
"	\"v\": { \"a\": 0.242676, \"r\": 0.00979627, \"n\": 0.0173772, \"d\": 0.00641566, \"c\": 0.0162178, \"q\": 0.0201581, \"e\": 0.0209253, \"g\": 0.031377, \"h\": 0.02005, \"i\": 0.324012, \"l\": 0.160912, \"k\": 0.0132994, \"m\": 0.0543241, \"f\": 0.0119872, \"p\": 0.0169276, \"s\": 0.0386037, \"t\": 0.131422, \"y\": 0.010484 }\n"
"    },\n"
"    \"insrate\" : 0.1,\n"
"    \"delrate\" : 0.1,\n"
"    \"insextprob\" : 0.9,\n"
"    \"delextprob\" : 0.9\n"
"}\n"
;
