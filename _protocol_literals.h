// Messages (Stored in the program memory) -- HTTP Header Part
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER01[] =  "POST ";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER02[] =  " HTTP/1.1";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER03[] =  "Content-Type: text/xml charset=UTF-8";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER04[] =  "User-Agent: PFFIAPUploadAgent (Arduino HCU)";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER05[] =  "Host: ";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER06[] =  "SOAPAction: \"http://soap.fiap.org/data\"";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER07[] =  "Content-Length: ";

// Messages (Stored in the program memory) -- HTTP Body Part
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY01[] =  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY02[] =  "<soapenv:Envelope xmlns:soapenv=";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY03[] =  "\"http://schemas.xmlsoap.org/soap/envelope/\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY04[] =  "<soapenv:Body>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY05[] =  "<ns2:dataRQ xmlns:ns2=\"http://soap.fiap.org/\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY06[] =  "<transport xmlns=\"http://gutp.jp/fiap/2009/11/\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY07[] =  "<body>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY10[] =  "<point id=\"";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY11[] =  "\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY12[] =  "<value time=\"";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY13[] =  "\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY14[] =  "</value>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY15[] =  "</point>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY17[] =  "</body>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY18[] =  "</transport>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY19[] =  "</ns2:dataRQ>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY20[] =  "</soapenv:Body>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY21[] =  "</soapenv:Envelope>";

PROGMEM const char FIAPUploadAgent_Post_TimeFormat[] = "%04d-%02d-%02dT%02d:%02d:%02d%s\0";
