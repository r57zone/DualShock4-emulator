IniReader
----------------
Sample:
    
    #include "stdafx.h"
    #include "IniReader\IniReader.h"
    #include <Windows.h>
    #include <iostream>
    #include <fstream>
    
    int main()
    {
        CIniReader ini1("ini1.ini");
        ini1.WriteInteger("MAIN", "INTEGER", 33);
        ini1.WriteFloat("MAIN", "FLOAT", 35.5f);
        ini1.WriteBoolean("MAIN", "BOOL", false);
        ini1.WriteString("MAIN", "STRING", "text");
    
        CIniReader ini2("ini2.ini");
        ini2.WriteInteger("MAIN", "INTEGER", 33);
        ini2.WriteFloat("MAIN", "FLOAT", 35.5f);
        ini2.WriteBoolean("MAIN", "BOOL", false);
        ini2.WriteString("MAIN", "STRING", "text");
    
        std::cout << ini1.ReadInteger("MAIN", "INTEGER", 0) << std::endl;
        std::cout << ini2.ReadString("MAIN", "STRING", "") << std::endl;
    
        if (ini1 == ini2)
            std::cout << "ini1 and ini2 are the same." << std::endl;
    
        std::ifstream file("ini1.ini", std::ios::in);
        std::stringstream ini_mem;
        ini_mem << file.rdbuf();
    
        CIniReader mem_ini(ini_mem);
    
        mem_ini.WriteInteger("MAIN", "INTEGER", 0);
    
        if (!mem_ini.CompareByValues(ini2))
            std::cout << "mem_ini and ini2 are different." << std::endl;
    
        return 0;
    }

Result:

![](http://i.imgur.com/LyqVYN7.png)