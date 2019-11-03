#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

using namespace std;

std::ifstream::pos_type filesize(string filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

template <class T>
int numDigits(T number)
{
    int digits = 0;
    if (number < 0) digits = 1; // remove this line if '-' counts as a digit
    while (number) {
        number /= 10;
        digits++;
    }
    return digits;
}

std::vector<std::string> explode(std::string const & s, char delim)
{
    std::vector<std::string> result;
    std::istringstream iss(s);

    for (std::string token; std::getline(iss, token, delim); )
    {
        result.push_back(std::move(token));
    }

    return result;
}

void insertStringToUTF16(string str, ofstream& file)
{
    for(int a=0; a<str.size(); a++)
    {
        file.put(str[a]);
        file.put(0x0);
    }
}

void MSG2TXT(string msgFile)
{
    ///Cut away the directory and extension
    string dir = msgFile.substr(0,msgFile.find_last_of("\\/")+1);
    string file = msgFile.substr(msgFile.find_last_of("\\/")+1);
    string file_noex = file.substr(0,file.find_last_of("."));
    string file_newex = dir+file_noex+".txt";

    //cout << dir << endl;
    //cout << file << endl;
    //cout << file_noex << endl;
    //cout << file_newex << endl;

    ///Important values
    uint32_t msgs;
    uint32_t magic;

    vector<uint32_t> offsets;
    vector<string> messages;

    ///Open the file
    ifstream f(msgFile, ios::binary);

    ///Read amount of messages
    f.seekg(0x0);
    f.read(reinterpret_cast<char*>(&msgs), sizeof(uint32_t));

    ///Read the magic value
    f.seekg(0x4);
    f.read(reinterpret_cast<char*>(&magic), sizeof(uint32_t));

    ///Add all message offsets to the vector
    for(uint32_t i=0; i<msgs; i++)
    {
        uint32_t offset;
        f.seekg(0x8+(i*4));
        f.read(reinterpret_cast<char*>(&offset), sizeof(uint32_t));
        offsets.push_back(offset);
    }

    ///Read and store messages
    for(uint32_t i=0; i<offsets.size(); i++)
    {
        ///Length of the message
        uint32_t length;

        ///If last element
        if(i == offsets.size() - 1)
        length = filesize(msgFile) - offsets[i];
        else ///if not
        length = offsets[i+1] - offsets[i];

        ///Initialize message container
        string s_msg;

        s_msg.resize(length);

        ///Read the message
        f.seekg(offsets[i]);
        f.read(&s_msg[0],length);

        ///Store in table
        messages.push_back(s_msg);

        //cout << "i: " << i << "offset: " << offsets[i] << " length: " << length << " string: " << s_msg.size() << endl;
    }

    f.close();

    ///Open TXT file
    ofstream of(file_newex, ios::binary);

    ///Put UTF16-LE BOM
    of.put(0xFF);
    of.put(0xFE);

    ///Get amount of digits
    int digits = numDigits(messages.size()-1);

    ///Prepare settings
    string settings = "SETTINGS:"+to_string(magic)+","+to_string(messages.size());

    ///Insert the settings
    insertStringToUTF16(settings,of);

    ///Add new line
    of.put('\r');
    of.put(0x0);
    of.put('\n');
    of.put(0x0);

    ///Write messages
    for(uint32_t i=0; i<messages.size(); i++)
    {
        ///Prepare the string
        string num = to_string(i);
        while(num.size() < digits)
        num = "0"+num;
        num += ":";

        ///Insert the string
        insertStringToUTF16(num,of);

        ///Insert the message
        of << messages[i];

        ///Place a new line
        of.put('\r');
        of.put(0x0);
        of.put('\n');
        of.put(0x0);
    }
}

void TXT2MSG(string txtFile)
{
    ///Cut away the directory and extension
    string dir = txtFile.substr(0,txtFile.find_last_of("\\/")+1);
    string file = txtFile.substr(txtFile.find_last_of("\\/")+1);
    string file_noex = file.substr(0,file.find_last_of("."));

    if(file.find(".") == std::string::npos)
    file_noex = file;

    string file_newex = dir+file_noex+".msg";

    //cout << dir << endl;
    //cout << file << endl;
    //cout << file_noex << endl;
    //cout << file_newex << endl;

    ///Open text file
    ifstream f(txtFile,ios::binary);

    ///Declare file size
    int fsize = filesize(txtFile);

    string settings = "";
    uint32_t start_offset = 0;

    ///Find messages, start from 2 to skip BOM
    for(int i=2; i<fsize; i+=2)
    {
        f.seekg(i);

        char c;
        f.get(c);

        if(c == 0x0D)
        {
            f.seekg(i+2);

            char cc;
            f.get(cc);

            if(cc == 0x0A)
            {
                ///Settings string found, breaking
                start_offset = i+4;
                break;
            }
        }

        settings += c;
    }

    ///Parse settings
    string s = settings.substr(settings.find_first_of(":")+1);
    vector<string> params = explode(s,',');

    //cout << "Magic: " << params[0] << " Messages: " << params[1] << endl;
    uint32_t magic = atoi(params[0].c_str());
    uint32_t digits = numDigits(atoi(params[1].c_str()));
    uint32_t msgoff = (digits + 1) * 2;
    uint32_t messages = atoi(params[1].c_str());
    vector<vector<char>> full_messages;

    ///Read messages from TXT file
    for(int i=0; i<messages; i++)
    {
        start_offset += msgoff;
        vector<char> tmp_msg;

        char lasta,lastb;

        while(true)
        {
            char a,b;
            f.seekg(start_offset);
            f.get(a);
            f.seekg(start_offset+1);
            f.get(b);

            lasta = a;
            lastb = b;

            tmp_msg.push_back(a);
            tmp_msg.push_back(b);

            ///Check if message doesn't end here
            f.seekg(start_offset+2);

            char c;
            f.get(c);

            if(c == 0x0D)
            {
                f.seekg(start_offset+4);

                char cc;
                f.get(cc);

                if(cc == 0x0A)
                {
                    ///End of message found, breaking
                    start_offset += 6;
                    break;
                }
            }

            start_offset += 2;
        }

        ///Add additional null bytes if there aren't any
        if(lasta != 0x0)
        {
            tmp_msg.push_back(0x0);
            tmp_msg.push_back(0x0);
        }

        //cout << tmp_msg.size() << endl;
        full_messages.push_back(tmp_msg);
    }

    f.close();

    ///Calculate header size
    int header_size = 8 + (full_messages.size()*4) + 4;

    ///Prepare offsets
    vector<uint32_t> offsets;
    offsets.push_back(header_size);

    for(int i=0; i<full_messages.size()-1; i++)
    {
        offsets.push_back(offsets[i]+full_messages[i].size());
        //cout << offsets[i] << " " << full_messages[i].size() << endl;
    }

    ///Open MSG file
    ofstream of(file_newex, ios::binary);

    ///Write header
    of.write((char*)&messages,sizeof(messages));
    of.write((char*)&magic,sizeof(magic));

    for(int i=0; i<offsets.size(); i++)
    {
        of.write((char*)&offsets[i],sizeof(offsets[i]));
    }

    of.put(0x0);
    of.put(0x0);
    of.put(0x0);
    of.put(0x0);

    ///Write messages
    for(int a=0; a<full_messages.size(); a++)
    {
        for(int b=0; b<full_messages[a].size(); b++)
        {
            of.put(full_messages[a][b]);
        }
    }

    of.close();
}

int main(int argc, char *argv[])
{
    cout << "MSGTools 3.0 by Owocek" << endl;

    if(argc <= 1)
    {
        cout << "Usage: msgtools.exe <file1> [file2, file3...]";
        return 1;
    }

    vector<string> files;

    if(argc >= 2)
    {
        for(int i=0; i<argc-1; i++)
        {
            files.push_back(argv[i+1]);
        }
    }

    for(int i=0; i<files.size(); i++)
    {
        ///Cut away the directory and extension
        string dir = files[i].substr(0,files[i].find_last_of("\\/")+1);
        string file = files[i].substr(files[i].find_last_of("\\/")+1);
        string file_ex = file.substr(file.find_last_of(".")+1);

        //cout << dir << endl;
        //cout << file << endl;
        //cout << file_ex << endl;

        if(file_ex == "txt")
        {
            TXT2MSG(files[i]);
            cout << "Successfully converted " << files[i] << " to MSG!" << endl;
        }
        else
        {
            MSG2TXT(files[i]);
            cout << "Successfully converted " << files[i] << " to TXT!" << endl;
        }
    }

    return 0;
}
