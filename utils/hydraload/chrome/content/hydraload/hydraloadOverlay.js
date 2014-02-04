var linkSubmitWindow;

var host = "127.0.0.1";
var port = "9999";

var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);					
if (prefs.getPrefType("hydraload.host") == prefs.PREF_STRING) {
	host = prefs.getCharPref("hydraload.host");
}
if (prefs.getPrefType("hydraload.port") == prefs.PREF_INT) {
	port = prefs.getIntPref("hydraload.port");
}	  			  			
  				

function tryDownload() {		
	focused = document.commandDispatcher.focusedElement;
		
	//alert("DEBUG: tagName=" + focused.tagName);
		
	switch (focused.tagName) {
		//currently only links are supported...
		case "A":					
			link = focused.toString();
			startDownload(link);							
			//linkSubmitWindow = window.open(
			//	"chrome://hydraload/content/linkSubmitWindow.xul", 
			//	"linkSubmitWindow", 
			//	"chrome, centerscreen"
			//);				   
			break;
			
		default: 	
			alert("Please right-click over a link...");	
	}		
}


function startDownload(link) {
	doRequest("modprobe http");
	doRequest("do " + link);
}


function createSubmitWindow(win) {		
	text = "Trying to download: " + link;
	
	info = linkSubmitWindow.document.getElementById("submit-text");	
	info.setAttribute("value", text);
}


function parseInputData(data) {	
	//TODO: really parse the data ;-)
	//response = linkSubmitWindow.document.getElementById("submit-response");
	//response.setAttribute("value", data);
}


function doRequest(request) {
	var listener = {
		finished : function(data) {
			parseInputData(data);        			
		}
	}
	request += "  \r\n";	
	//alert("Host: " + host);
	//alert("Port: " + port);
	//alert("Request: " + request);
	sendData(host, port, request, listener);
}


function sendData(h, p, outputData, listener) {
	try {
		var transportService = Components.classes["@mozilla.org/network/socket-transport-service;1"].getService(Components.interfaces.nsISocketTransportService);
		var transport = transportService.createTransport(null, 0, h, p, null);

		var outstream = transport.openOutputStream(0, 0, 0);
		outstream.write(outputData, outputData.length);

		var stream = transport.openInputStream(0, 0, 0);
		var instream = Components.classes["@mozilla.org/scriptableinputstream;1"].createInstance(Components.interfaces.nsIScriptableInputStream);
		instream.init(stream);

		var dataListener = {
			data : "",
			onStartRequest: function(request, context) {},
			onStopRequest: function(request, context, status) {
				instream.close();
				outstream.close();
				listener.finished(this.data);
			},
			onDataAvailable: function(request, context, inputStream, offset, count) {
				this.data += instream.read(count);
			},
		};

		var pump = Components.classes["@mozilla.org/network/input-stream-pump;1"].createInstance(Components.interfaces.nsIInputStreamPump);
		pump.init(stream, -1, -1, 0, 0, false);
		pump.asyncRead(dataListener, null);
	} catch (ex) {
		return ex;
	}
	
	return null;
}
