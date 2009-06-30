../dspam-thunderbird-extension.xpi: chrome/dspam.jar
	zip ../dspam-thunderbird-extension.xpi * chrome/dspam.jar -x Makefile -x README -x TODO
	rm chrome/dspam.jar

chrome/dspam.jar:
	cd chrome && zip -r dspam.jar content locale skin && cd ..

clean:
	rm -f ../dspam-thunderbird-extension.xpi chrome/dspam.jar
