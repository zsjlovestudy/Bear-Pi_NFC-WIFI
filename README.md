# Bear-Pi_NFC-WIFI
小熊派nfc碰一碰wifi

可以实现nfc写记录进去连接wifi。
代码存在不少潜在bug：
1.wifi账号密码最大长度不知
2.关于nfc和wifi的数组长度没有确定准确长度
3.没有判断越界

How to use:
使用NFC Tools写一个文本记录进去
格式：
{"ssid":"ssid","key":"key"}
