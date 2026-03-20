export function readUshort(buff,p)   {  return (buff[p]) | (buff[p+1]<<8);  }
export function writeUshort(buff,p,n){  buff[p] = (n)&255;  buff[p+1] = (n>>8)&255;  }
export function readUint(buff,p)     {  return (buff[p+3]*(256*256*256)) + ((buff[p+2]<<16) | (buff[p+1]<< 8) | buff[p]);  }
export function writeUint(buff,p,n){  buff[p]=n&255;  buff[p+1]=(n>>8)&255;  buff[p+2]=(n>>16)&255;  buff[p+3]=(n>>24)&255;  }
export function readASCII(buff,p,l){  var s = "";  for(var i=0; i<l; i++) s += String.fromCharCode(buff[p+i]);  return s;    }
export function writeASCII(data,p,s){  for(var i=0; i<s.length; i++) data[p+i] = s.charCodeAt(i);  }
export function pad(n) { return n.length < 2 ? "0" + n : n; }
export function readUTF8(buff, p, l) {
	var s = "", ns;
	for(var i=0; i<l; i++) s += "%" + pad(buff[p+i].toString(16));
	try {  ns = decodeURIComponent(s); }
	catch(e) {  return readASCII(buff, p, l);  }
	return  ns;
}
export function writeUTF8(buff, p, str) {
	var strl = str.length, i=0;
	for(var ci=0; ci<strl; ci++)
	{
		var code = str.charCodeAt(ci);
		if     ((code&(0xffffffff-(1<< 7)+1))==0) {  buff[p+i] = (     code     );  i++;  }
		else if((code&(0xffffffff-(1<<11)+1))==0) {  buff[p+i] = (192|(code>> 6));  buff[p+i+1] = (128|((code>> 0)&63));  i+=2;  }
		else if((code&(0xffffffff-(1<<16)+1))==0) {  buff[p+i] = (224|(code>>12));  buff[p+i+1] = (128|((code>> 6)&63));  buff[p+i+2] = (128|((code>>0)&63));  i+=3;  }
		else if((code&(0xffffffff-(1<<21)+1))==0) {  buff[p+i] = (240|(code>>18));  buff[p+i+1] = (128|((code>>12)&63));  buff[p+i+2] = (128|((code>>6)&63));  buff[p+i+3] = (128|((code>>0)&63)); i+=4;  }
		else throw "e";
	}
	return i;
}
export function sizeUTF8(str) {
	var strl = str.length, i=0;
	for(var ci=0; ci<strl; ci++)
	{
		var code = str.charCodeAt(ci);
		if     ((code&(0xffffffff-(1<< 7)+1))==0) {  i++ ;  }
		else if((code&(0xffffffff-(1<<11)+1))==0) {  i+=2;  }
		else if((code&(0xffffffff-(1<<16)+1))==0) {  i+=3;  }
		else if((code&(0xffffffff-(1<<21)+1))==0) {  i+=4;  }
		else throw "e";
	}
	return i;
}
