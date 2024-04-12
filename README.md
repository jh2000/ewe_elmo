# Eine für EWEs "ELMO" alternative Firmware
In diesem Repo stelle ich meine Erkenntnisse zu ELMO, ein vom Netzbetreiber EWE benutztes Dongle an elektronischen Zählern (mME, "moderne Messeinrichtung"), allen zur Verfügung.<br>

<b>Der Auslieferungszustand:</b><br>
EWE Elmo ist Gratis für alle EWE-Stromkunden bestellbar und liefert eine Echtzeitauswertung des Stromverbrauchs bzw. der Erzeugung.<br>
Ein dynamischer Stromtarif ist damit nicht möglich.<br>
Die Originalfirmware erlaubt auf der Weboberfläche das WLAN einzurichten und auf /data die reinen Zählerdaten und /reported allgemeinere Informationen anzuzeigen.<br>
Damit lassen sich für die meisten Projekte ausreichende Daten abfragen.<br>
Ein geiles Feature und Alleinstellungsmerkmal ist, dass ELMO selbstständig die PIN für manche Zähler eingeben kann.<br>
Das Dongle wird per integriertem Magnet am Zähler befestigt und via USB mit Strom versorgt. Nach der Einrichtung via EWE App ist es mit dem EWE-Account gekoppelt und zeigt einem die zugehörigen Vertragsdaten, den Livestromverbrauch usw... an.<br>

Wenn man den Stromanbieter wechselt verliert Elmo nicht direkt seine Funktion, die App ist aber unmittelbar nach dem Wechsel nicht mehr Benutzbar (kein Login mehr Möglich) und nach ein paar Wochen habe ich Elmo mal neu gestartet und daraufhin hat er dann den Dienst teilweise verweigert.<br>
Auf den lokalen Seiten wie /data stehen zwar die aktuellen Zählerstande zur Verfügung und unter /reported kann man auch sehen, dass Elmo die PIN noch weis, aber sie wird nicht mehr eingegeben.<br>
Zurückgeben muss man Elmo übrigens nicht.<br>
<br>
Das hat mich dazu bewegt Elmo nun doch zu zerlegen und weitergehend zu Analysieren um eine mögliche Lösung herbeizuführen und die unnötigen Datenanfragen an EWE abzukoppeln.<br>
<br>
<b>Technische Daten:</b><br>
Das Gerät wird von Firma BSED GmbH als nanoBeemsPro gebaut und vermutlich von weiteren Stromanbietern vertrieben.<br>
Stromversorgung erfolgt über Micro-USB (Reine Stromversorgung, kein RS232-Pegelwandler an Bord)<br>
2 LEDs nach Außen für WLAN Status und Zählerkommunikationsstatus<br>
2 LEDs (Dual-LED, Weis+Rot) Richtung Zähler u.a. zum "Einblinken" der Pin<br>
1 IR-Led zum Pull Betrieb am Zähler (Falls dieser nicht Periodisch sendet) oder auch zum "Einblinken" der Pin<br>
1 IR-Fototransistor zum Lesen der Zählerdaten<br>
1 Taster neben dem Micro-USB Anschluss (Bei mir zum starten der PIN-Sequenz benutzt, welche den INFO-Parameter umschaltet auf 'vollständigen Datensatz')<br>
An Bord ist ein ESP32-WROOM-32U Microcontroller<br>
<br>
Bei den zum Zähler gerichteten LEDs gibt es nur die Möglichkeit "Aus", Weiß ODER Rot, da es eine 2 Pin LED ist, welche an 2 Pins vom ESP32 hängt.<br>
<br>
Die WLAN-Zugangsdaten werden aus der Serialnummer abgeleitet.<br>
Dabei ist die SSID "Sensor_" gefolgt von den ersten 4 Stellen des MD5-Hashes von der Serialnummer inklusive aller Buchstaben in Uppercase (also bei SN ABCD0123456789 "3bbc52eeb81ed5243f238ec254bdc9c1" -> 3bbc -> Sensor_3bbc)<br>
Das Passwort sind die nächsten 10 Stellen, also entsprechend im Beispiel 52eeb81ed5.<br>
<br>
Im NVS finden sich die Seriennummer, ein shared Key für den Azure Endpunkt und ein paar persistente Einstellungen.<br>
<br>
Das Gehäuse lässt sich einfach mit etwas druck/zug öffnen. Die Platine selbst hat Abseits der Revisionsnummer keine Beschriftungen.<br>
<br>
Leider gibt es keinen "guten" Zugriff auf die serielle Schnittstelle. Es sind lediglich Testpunkte auf der Platine vorhanden welche alle relevanten Funktionen erreichbar machen.<br>
<br>
Hängt man sich an die Pins lässt sich das angehängte Bootlog sehen. Zudem werden alle möglichen Zugriffe und Funktionen ausgegeben.<br>
Da ich dies erst gemacht habe als mein EWE Account bereits deaktiviert war kann ich nicht sagen was da noch "geht".<br>
Ich habe relativ schnell die Firmware mit einem eigenen Programm überschrieben ohne vorher ein Abbild zu machen. Vielleicht ist jemand etwas klüger und macht vorher einen Dump (falls es nicht gesperrt ist) und fügt ihn dem Repo hinzu.<br>
Zum reinen umflashen braucht man also nur kurz eine ruhige Hand und kann ganz ohne Löten die Kabel an die Testpunkte halten und eine OTA fähige Software aufspielen.<br>
Um den normalen Bootvorgang zu unterbrechen und mit ESPTool arbeiten zu können muss man während des Einschaltvorganges den Pin 25 (IO0) auf GND ziehen.<br>
<br>
Wichtig für das Flashen sind die SPI korrekten Einstellungen:<br>
Der SPI Flash ist 8MB (64Mbit) groß, SPI Mode muss DIO sein.<br>
<br>
Ich habe einen EMH eHZ-P. Dieser reagiert für das Menü bzw. die PIN-Eingabe weder auf das Rot noch auf das Weis von ELMO. Er reagiert aber auch die IR-Sendediode welche bei diesem Zählertypen somit entsprechend benutzt wird (die anderen Farben lasse ich zur Verdeutlichung trotzdem leuchten fürs Debugging der Sequenz).

