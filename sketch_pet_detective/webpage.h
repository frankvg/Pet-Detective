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
base64 data so we could still display a shortcut icon and a larger inverted
paw print for the backgound (see near the bottom of this page for more details).

11-Mar-2026 (c) FAQware

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
window.onload = function() {
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
  }
  setInterval(refreshCheck, 6000);	
};
</script>
<!-- <meta http-equiv="refresh" content="30"> -->
<style>
:root {
	--pet-color:#)HTML_PAGE";

const String webPageMiddle = R"HTML_PAGE(;
}
body {
background-color: #000;
}
* {
margin: 0;
padding:0;
box-sizing:border-box;
font-family: Verdana; sans-serif;
text-align: center;
color: #6A67EF;
font-size: 20px;	
}
.module {
position: relative;
}
.module::before {
content: "";
position: absolute;
top: 0; left: 0;
width: 100%; height: 100%;
background-image: url()HTML_PAGE";

const String webPageAfterPaw = R"HTML_PAGE(deg) brightness(80%);
background-color: #000000;
background-repeat: repeat-x;
background-position: 0px 300px;
}
.module-inside {
position: relative;
}
.stat-box {
border: solid var(--pet-color);
border-radius: 10px;
margin: 0 auto;
padding: 0 57px 20px;
font-style: italic;
width: fit-content;
background-color: black;
}
.stat-now {
font-style:normal;
font-weight:bold;
font-size: 54px;
color: var(--pet-color);
padding-bottom: 18px;
}
.stat-at {
font-style:normal;
font-size: 54px;
color: var(--pet-color);
opacity: .8;
padding-bottom: 15px;
}
.stat-grey {
font-style:normal;
font-size: 33px;
color: #BBB;
padding-bottom: 15px;
}
.stat {
transform: translatey(-60%);
width: max-content;
font-style:italic;
padding: 0 12px;
color: #CCC;
font-size: 24px;
background-color: black;	
}
.gradient-text {
font-size: 82px; 
background-image: linear-gradient(to bottom, #FFF, #4641CC); 
-webkit-background-clip: text;
-webkit-text-fill-color: transparent;
display: inline-block; 
font-style:italic;
font-weight: 100;
font-family: "Poetsen One", sans-serif; 
padding-right:30px;
padding-left:30px;
margin-top: 20px;
margin-bottom: 25px;
}  
.log-box {
border: solid #8000FF;
padding-right:30px;
padding-left:30px;
}
table th {
color: #3372FF;
text-align: center;
font-style:normal;
font-size: 20px;
}
table td {
color: #A0A0C0;
font-style:normal;
font-size: 20px;
padding-right:8px;
padding-left:8px;
}
.container {
display: flex;
flex-wrap: nowrap;
}
.left-section {
}
.right-section {
padding-left:20px;
width: 246;
}
</style>
</head>
<body>
<div class="module">
<div class="module-inside">
<h1 class="gradient-text">Pet Detective</h1>
<div class="stat-box">
<div class="stat">Status</div>
<div class="container">
<div class="left-section">)HTML_PAGE";

const String webPageBottom1 = R"HTML_PAGE(
</div>
</div>
</div>
</div>
Copyright 2026 FAQware<br>)HTML_PAGE";

const String webPageBottom2 = R"HTML_PAGE(
</div>
</div>
</body>
</html>)HTML_PAGE";

// This is a single paw png image used in the repeating background line. It 
// also has the hue filter. We change the hue rather than having 4 different 
// colored paw images

const String pawImage = "data:image/png;base64,"
"iVBORw0KGgoAAAANSUhEUgAAACwAAAAsCAMAAAApWqozAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSB"
"JbWFnZVJlYWR5ccllPAAAAGBQTFRF/7QAAAAApnUA15gAmWwAZUcANycA6aUAGxMAdVMASzUA86"
"sAJxsAxIoAUjoAu4QA/bMAjWQAhF0AEg0ADAgABgQACQcAXUIAAwIAz5IAQy8AAgEAAQEA354A+"
"bAABAMAJqidogAAAaZJREFUeNrUVd26gyAMo4r4P6ronBPd+7/lASluO8J2vV4B5oshaZWxny9e"
"DNV67OZk/oQFU4oQ+V0DlFFsrSwYmm3HNvtGxsAJuOrs5uLWdwxjsSRwYzaS1tcI2JMBpIxVmtY"
"1fgEnrB5pqesIc6YP0V4FqJhm7tl6dizvMTdST8dZB6/OvHOmSWtfJ4iuPZiHPOTZImZkctkRBb"
"KZ5Pf/JaMLY2i909wcuvzEdlJBby9qxHKEpbRsrfVO5PbaQt3bJ3ggqbZrckldN2dTYvulsk+G9"
"AD7AIZzAEhJTsdJQeBHEm5aK/EZs46ZigU9up1ak1p9kx0n4UfqT+atoaN+N9Kq0pfcetJ78Evq"
"8uaOjNP77fcOMjOCGW2WV4H8sUeAZpi8fhjNdbNQ6tgKvVS5Hzxn+9hiF7l5bUyuBbyWWlcXwRR"
"q6gneSyA3rTWW+GG6n9Wz9LDxvXJ1Al/X2KBkcK4sMoLrIwB+5GFwqQNg6ILUWxHCQni65yAxqO"
"BHQwaxWoWZg2DIvjg3ipKX7ga6in3q5NQUoupl7WRlhRIcf/3H9yfAADLIEzo6PvVqAAAAAElFT"
"kSuQmCC"
");\n"
"filter: hue-rotate(";
