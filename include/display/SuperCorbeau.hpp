namespace display {
  namespace superCorbeau {
    /*  GIMP header image file format (RGB). Arranged a bit by kellen_j*/

    constexpr static unsigned int width = 20;
    constexpr static unsigned int height = 20;

    /*  Call this function repeatedly.  After each use, the pixel data can be extracted  */

    inline void headerPixel(char const *data, unsigned char *pixel) {
      pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4));
      pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2));
      pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33)));
      data += 4;
    }

    constexpr static char const *header_data =
      "````````^04U5V.4!Q-$'BI;?(BYK[OLUN,3`@X^````````````````````````"
      "````````````````````````H*S=!!!!!!!!!!!!&R=8L;WNU^04YO,C````````"
      "````````````````````````````````````````15&\"!!!!!!!!!!!!!!!!2E:'"
      "FJ;7`@X^````````````````````````````````````````````[_PL!!!!!!!!"
      "!!!!\"Q=(S-D)````````````````````````````````````````````````````"
      "````B)3%!!!!!!!!!!!!$1U.`P\\_````````````````````````````````````"
      "````````````````Q=(\"!1%\"!!!!!!!!!!!!$Q]0````TM\\/ML+SNL;W[_PL`P\\_"
      "`P\\_````````````````````````````+#AI!!!!!!!!!!!!!!!!$Q]0````7&B9"
      "#AI+K+CIM,#QZ_@H`@X^G*C9````````````````````````%2%2!!!!!!!!!!!!"
      "!!!!$Q]0````86V>!!!!3UN,`P\\_`````P\\_3%B)````````````````````````"
      "#AI+!!!!!!!!!!!!!!!!$Q]0````86V>!!!!)C)C_PL[`````P\\_.T=X````````"
      "````````````````3%B)!!!!!!!!!!!!!!!!$Q]0````A9'\".T=X04U^^@8V````"
      "T=X.$AY/````````````````````````VN<7T-T-!!!!!!!!!!!!$Q]0````````"
      "````````````K+CI!A)#I+#A````````````````````````````````K[OL!1%\""
      "!!!!$Q]0````;WNL&\"15(2U>=(\"Q````Q-$!````````````````````````````"
      "````````````#!A)!1%\"%2%2````86V>!!!!!!!!!!!!E:'2`@X^O,CY````````"
      "````````````````````````````GZO<,3UNXN\\?````86V>!!!!!!!!!!!!(R]@"
      "````F*35`0T]````````````````````````````````````!!!!````````86V>"
      "!!!!!!!!!!!!!!!!HJ[?````:W>H````````````````````````````````````"
      "1E*#````````:'2E!!!!2E:'$1U.!!!!1E*#_`P\\U^04ZO<G````````````````"
      "````````````R]@(PL[_````I;'B.$1UML+SR]@(````N,3U````T=X.+#AIKKKK"
      "````````````````````````^04U=(\"Q`````````@X^`0T]^@8VHJ[?````````"
      "`````````0T]`0T]````````````````]`0T[/DI>86V=H*SL+SM````````\\_`P"
      "DI[/3%B)\\/TM````````````````````````````````````[_PL>H:WIK+CY?(B"
      "````````````^`@XGZO<YO,C_PL[````````````````````````````````````"
      "";
  }
}

