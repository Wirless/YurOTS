var request = new ActiveXObject("Msxml2.XMLHTTP");
var notyetready = 1;
request.onreadystatechange=function()
{
    if(request.readyState==4)
    {
        WScript.Echo(request.responseText);
        notyetready = 0;
    }
}
request.open( "GET", "http://www.whatismyip.com/automation/n09230945.asp", true );
request.send(null);
while( notyetready )
{
    WScript.Sleep( 100 );
}