(This file is written in Japanese, Shift_JIS charset.)

This library is forked from FIAPUploadAgent for use plantfactory project.

Arduino用のIEEE1888ライブラリ FIAPUploadAgent は以下のファイルからなっています。

FIAPUploadAgent
 +-- examples
 |    +-- FIAPsensorsAPIHowto
 |    |    ^-- FIAPsensorsAPIHowto.ino   … IEEE1888ライブラリの簡単なサンプル
 |    +-- FIAPsensorsCLI
 |         ^-- FIAPsensorsCLI.ino   … 学習キットのセンサデータをサーバに送信するサンプル（CLI版）
 +-- FIAPUploadAgent.cpp    … IEEE1888ライブラリの本体
 +-- FIAPUploadAgent.h      … そのヘッダファイル
 +-- keywords.txt
 +-- readme.txt

※実行時には、次のライブラリをダウンロードし、Arduinoのlibrariesディレクトリに配置してください。

・Timeライブラリ
　　http://www.arduino.cc/playground/Code/Time

※動作テスト用サーバ

・アップロードされたデータの閲覧
　　http://fiap-sandbox.gutp.ic.i.u-tokyo.ac.jp/
・IEEE1888通信URL
　　http://fiap-sandbox.gutp.ic.i.u-tokyo.ac.jp/axis2/services/FIAPStorage

※IEEE1888サーバの入手方法

　http://gutp.jp/fiap/kit.html
