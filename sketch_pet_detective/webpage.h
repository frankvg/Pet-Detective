/*  Pet Detective, by FAQware

Status Web Page

The HTML code is broken up into sections where custom data is inserted when the 
page is built and sent to the requesting browser.

There is a small amount of JavaScript to check with the Pet Detective 'server' every 
6 seconds to see if any transitions have occurred.  If a transition has occured, a 
full page refresh is requested by the JavaScript code. Otherwise, only 26 bytes are 
transfered every 6 seconds. When a full page refresh occurs, it can take several 
seconds for our slow ESP32 processor to generate the page.

Since we don't have a file system, we converted our 32-bit paw-print icon into 
base64 data so we could still display a shortcut icon.

11-Feb-2026 (c) FAQware

*/

const String webPageTop = R"HTML_PAGE(
<!doctype html>
<html>
<head>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Poetsen+One&display=swap" rel="stylesheet">
<link rel="shortcut icon" href="data:image/x-icon;base64,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
ADAP/4B4H/+Afn//wf//" />
<title>Pet Detective</title>
<script>
async function refreshCheck() {
 showlog = "n";
 try {
  const queryString = window.location.search;
  if (queryString != null) {
   const urlParams = new URLSearchParams(queryString);
   if (urlParams != null) {
    if (urlParams.get('log') == "y") showlog = "y";
   }
  }
  const url = new URL(window.location.href);
  url.searchParams.set("log", showlog);
  url.searchParams.set("ver", ")HTML_PAGE";

const String webPageVersion = R"HTML_PAGE(");
  const response = await fetch(url.href);
  if (response.status == 205) { 
   // changed, so force refresh page
   url.searchParams.delete("ver");
   window.location.href = url.href;
  }
 } catch (error) {
   console.error(error.message);
 }
 //window.location.href = url.href;
}
setInterval(refreshCheck, 6000);	
</script>
<!-- <meta http-equiv="refresh" content="30"> -->
<style>
:root {
	--pet-color:#)HTML_PAGE";

const String webPageMiddle = R"HTML_PAGE(;
}
* {
margin: 0;
padding:0;
box-sizing:border-box;
background-color:#000;
font-family: Verdana; sans-serif;
text-align: center;
color: #6A67EF;
font-size: 12px;	
}
.stat-box {
border: solid var(--pet-color);
border-radius: 7px;
margin: 0 auto;
padding: 0 38px 20px;
font-style: italic;
width: fit-content;
}
.stat-now {
font-style:normal;
font-size: 36px;
color: var(--pet-color);
padding-bottom: 12px;
}
.stat-at {
font-style:normal;
font-size: 36px;
color: var(--pet-color);
opacity: .8;
padding-bottom: 10px;
}
.stat-grey {
font-style:normal;
font-size: 22px;
color: #BBB;
padding-bottom: 10px;
}
.stat {
transform: translatey(-60%);
width: max-content;
font-style:italic;
padding: 0 8px;
color: #CCC;
font-size: 16px;	
}
.gradient-text {
font-size: 48px; 
background-image: linear-gradient(to bottom, #FFF, #A5A0FE); 
-webkit-background-clip: text;
-webkit-text-fill-color: transparent;
display: inline-block; 
font-style:italic;
font-weight: 100;
font-family: "Poetsen One", sans-serif; 
padding-right:20px;
padding-left:20px;
margin-top: 20px;
margin-bottom: 30px;
}  
.log-box {
border: solid #8000FF;
padding-right:12px;
padding-left:12px;
}
table th {
color: #3372FF;
text-align: center;
font-style:normal;
font-size: 13px;
}
table td {
color: #A0A0C0;
font-style:normal;
font-size: 13px;
padding-right:5px;
padding-left:5px;
}

</style>
</head>
<body>
<h1 class="gradient-text">Pet Detective</h1>
<div class="stat-box">
<div class="stat">Status</div>)HTML_PAGE";

const String webPageBottom1 = R"HTML_PAGE(
</div>
Copyright 2026 FAQware<br>)HTML_PAGE";

const String webPageBottom2 = R"HTML_PAGE(
</body>
</html>)HTML_PAGE";
