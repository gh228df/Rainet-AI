#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>

using namespace std;
using namespace chrono;

const int indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

int firewallfir = -1; //0b ycoords(3bits) xcoords(3bits)
int firewallsec = -1; //0b ycoords(3bits) xcoords(3bits)

const int MIN = -1000000;
const int MAX = 1000000;

struct ttentry{
	int score;
	uint8_t flag;
};

struct field{
    uint64_t firl;
    uint64_t firv;
    uint64_t secl;
    uint64_t secv;
    // ints are used for simd instructions benefits
    int fir[8]; //0b 0 0 0 0 ycoords(3bits) xcoords(3bits)
    int sec[8]; //0b 0 0 0 0 ycoords(3bits) xcoords(3bits)
    int firlinkindex = 4;
    int firvirusindex = 8;
    int seclinkindex = 4;
    int secvirusindex = 8;
    bool isboostavailablefir = true;
    bool isboostavailablesec = true;
    bool isswapavailablefir = true;
    bool isswapavailablesec = true;
    bool ischeckeravailablefir = true;
    bool ischeckeravailablesec = true;
    bool isfirewallavailablefir = true;
    bool isfirewallavailablesec = true;
    int firvirus = 0;
    int firlink = 0;
    int secvirus = 0;
    int seclink = 0;
    int evaluatefir(){
        int res = (firlink << 10) - (firvirus << 11) - (seclink << 11) + (secvirus << 10);
        for(int i = 0; i < 8; ++i)
            res -= (fir[i] & 56);
        return res;
    }
    int evaluatesec(){
        int res = (secvirus << 11) - (seclink << 10) + (firlink << 11) - (firvirus << 10);
        for(int i = 0; i < 8; ++i)
            res -= (sec[i] & 56);
        return res;
    }
    size_t operator()(const field& s) const {
        return hash<uint64_t>()(s.firl | s.firv | s.secl | s.secv) ^ hash<uint64_t>()((fir[0] << 7) | (fir[4] << 14) | (sec[0] << 21) | sec[4]);
    }
    bool operator==(const field& other) const {
        if(firl == other.firl and firv == other.firv and 
        secl == other.secl and secv == other.secv and 
        isboostavailablefir == other.isboostavailablefir and 
        isboostavailablesec == other.isboostavailablesec and 
        isswapavailablefir == other.isswapavailablefir and 
        isswapavailablesec == other.isswapavailablesec and 
        ischeckeravailablefir == other.ischeckeravailablefir and
        ischeckeravailablesec == other.ischeckeravailablesec and 
        isfirewallavailablefir == other.isfirewallavailablefir and
        isfirewallavailablesec == other.isfirewallavailablesec and 
        firvirus == other.firvirus and
        firlink == other.firlink and 
        secvirus == other.secvirus and
        seclink == other.seclink){
            if(isboostavailablefir){
                if(isboostavailablesec)
                    return true; // 0 0 0 0
                if(sec[0] > 63) 
                    return sec[0] == other.sec[0]; //0 0 x 0
                return sec[4] == other.sec[4]; //0 0 0 x
            }
            if(isboostavailablesec){ 
                if(fir[0] > 63) 
                    return fir[0] == other.fir[0]; //x 0 0 0
                return fir[4] == other.fir[4]; //0 x 0 0
            }
            if(fir[0] > 63){
                if(sec[0] > 63)
                    return fir[0] == other.fir[0] and sec[0] == other.sec[0]; //x 0 x 0
                return fir[0] == other.fir[0] and sec[4] == other.sec[4]; //x 0 0 x
            }
            if(sec[0] > 63)
                return fir[4] == other.fir[4] and sec[0] == other.sec[0]; //0 x x 0
            return fir[4] == other.fir[4] and sec[4] == other.sec[4]; //0 x 0 x
        }
        return false;
    }
};

void printfield(field todisplay){
    cout << "Virus: " << todisplay.firvirus << "         Link: " << todisplay.firlink << endl;
    for(int i = 63; i > -1; --i){
        if((todisplay.firl >> i) & 1){
            for(int u = 0; u < todisplay.firlinkindex; ++u)
                if(todisplay.fir[u] > 0 and (todisplay.fir[u] & 63) == i and (todisplay.fir[u] & 64) == 64){
                    cout << "\033[32m[\033[0m\033[34mL\033[0m\033[32m]\033[0m";
                    goto end;
                }
            cout << "[\033[32mL\033[0m]";
        }
        else if((todisplay.firv >> i) & 1){
            for(int u = 4; u < todisplay.firvirusindex; ++u)
                if(todisplay.fir[u] > 0 and (todisplay.fir[u] & 63) == i and (todisplay.fir[u] & 64) == 64){
                    cout << "\033[32m[\033[0m\033[34mV\033[0m\033[32m]\033[0m";
                    goto end;
                }
            cout << "[\033[32mV\033[0m]";
        }
        else if((todisplay.secl >> i) & 1){
            for(int u = 0; u < todisplay.seclinkindex; ++u)
                if(todisplay.sec[u] > 0 and (todisplay.sec[u] & 63) == i and (todisplay.sec[u] & 64) == 64){
                    cout << "\033[31m[\033[0m\033[34mL\033[0m\033[31m]\033[0m";
                    goto end;
                }
            cout << "[\033[31mL\033[0m]";
        }
        else if((todisplay.secv >> i) & 1){
            for(int u = 4; u < todisplay.secvirusindex; ++u)
                if(todisplay.sec[u] > 0 and (todisplay.sec[u] & 63) == i and (todisplay.sec[u] & 64) == 64){
                    cout << "\033[31m[\033[0m\033[34mV\033[0m\033[31m]\033[0m";
                    goto end;
                }
            cout << "[\033[31mV\033[0m]";
        }
        else
            cout << "[ ]";
        end:
        if(i % 8 == 0)
            cout << endl;
    }
    cout << "Virus: " << todisplay.secvirus << "         Link: " << todisplay.seclink << endl;
}

void generatexfield(field &togenerate, const bool player, const int x){
    if(player){
        togenerate.firv = 0;
        togenerate.firl = 0;
        int linkindex = 0, virusindex = 4;
        if(x & 1){
            togenerate.fir[virusindex] = 56;
            togenerate.firv |= (1ULL << 56);
            virusindex++;
        }
        else
        {
            togenerate.fir[linkindex] = 56;
            togenerate.firl |= (1ULL << 56);
            linkindex++;
        }
        if(x & 2){
            togenerate.fir[virusindex] = 57;
            togenerate.firv |= (1ULL << 57);
            virusindex++;
        }
        else
        {
            togenerate.fir[linkindex] = 57;
            togenerate.firl |= (1ULL << 57);
            linkindex++;
        }
        if(x & 4){
            togenerate.fir[virusindex] = 58;
            togenerate.firv |= (1ULL << 58);
            virusindex++;
        }
        else
        {
            togenerate.fir[linkindex] = 58;
            togenerate.firl |= (1ULL << 58);
            linkindex++;
        }
        if(x & 8){
            togenerate.fir[virusindex] = 51;
            togenerate.firv |= (1ULL << 51);
            virusindex++;
        }
        else
        {
            togenerate.fir[linkindex] = 51;
            togenerate.firl |= (1ULL << 51);
            linkindex++;
        }
        if(x & 16){
            togenerate.fir[virusindex] = 52;
            togenerate.firv |= (1ULL << 52);
            virusindex++;
        }
        else
        {
            togenerate.fir[linkindex] = 52;
            togenerate.firl |= (1ULL << 52);
            linkindex++;
        }
        if(x & 32){
            togenerate.fir[virusindex] = 61;
            togenerate.firv |= (1ULL << 61);
            virusindex++;
        }
        else
        {
            togenerate.fir[linkindex] = 61;
            togenerate.firl |= (1ULL << 61);
            linkindex++;
        }
        if(x & 64){
            togenerate.fir[virusindex] = 62;
            togenerate.firv |= (1ULL << 62);
            virusindex++;
        }
        else
        {
            togenerate.fir[linkindex] = 62;
            togenerate.firl |= (1ULL << 62);
            linkindex++;
        }
        if(x & 128){
            togenerate.fir[virusindex] = 63;
            togenerate.firv |= (1ULL << 63);
        }
        else
        {
            togenerate.fir[linkindex] = 63;
            togenerate.firl |= (1ULL << 63);
        }
    }
    else
    {
        togenerate.secv = 0;
        togenerate.secl = 0;
        int linkindex = 0, virusindex = 4;
        if(x & 1){
            togenerate.sec[virusindex] = 0;
            togenerate.secv |= 1;
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 0;
            togenerate.secl |= 1;
            linkindex++;
        }
        if(x & 2){
            togenerate.sec[virusindex] = 1;
            togenerate.secv |= 2;
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 1;
            togenerate.secl |= 2;
            linkindex++;
        }
        if(x & 4){
            togenerate.sec[virusindex] = 2;
            togenerate.secv |= 4;
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 2;
            togenerate.secl |= 4;
            linkindex++;
        }
        if(x & 8){
            togenerate.sec[virusindex] = 11;
            togenerate.secv |= 2048;
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 11;
            togenerate.secl |= 2048;
            linkindex++;
        }
        if(x & 16){
            togenerate.sec[virusindex] = 12;
            togenerate.secv |= 4096;
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 12;
            togenerate.secl |= 4096;
            linkindex++;
        }
        if(x & 32){
            togenerate.sec[virusindex] = 5;
            togenerate.secv |= 32;
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 5;
            togenerate.secl |= 32;
            linkindex++;
        }
        if(x & 64){
            togenerate.sec[virusindex] = 6;
            togenerate.secv |= 64;
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 6;
            togenerate.secl |= 64;
            linkindex++;
        }
        if(x & 128){
            togenerate.sec[virusindex] = 7;
            togenerate.secv |= 128;
        }
        else
        {
            togenerate.sec[linkindex] = 7;
            togenerate.secl |= 128;
        }
    }
}

inline void removesecondlink(field &position, const int &coords){
    switch(position.seclinkindex)
    {
        case 1:
        if(position.sec[0] & 64)
            position.isboostavailablesec = true;
        --position.seclinkindex;
        position.sec[0] = 0;
        return;
        case 2:
        if((position.sec[0] & 63) == coords){
            if(position.sec[0] & 64)
                position.isboostavailablesec = true;
            --position.seclinkindex;
            position.sec[0] = position.sec[1];
            position.sec[1] = 0;
            return;
        }
        --position.seclinkindex;
        position.sec[1] = 0;
        return;
        case 3:
        if((position.sec[0] & 63) == coords){
            if(position.sec[0] & 64)
                position.isboostavailablesec = true;
            --position.seclinkindex;
            position.sec[0] = position.sec[2];
            position.sec[2] = 0;
            return;
        }
        if(position.sec[1] == coords){
            --position.seclinkindex;
            position.sec[1] = position.sec[2];
            position.sec[2] = 0;
            return;
        }
        --position.seclinkindex;
        position.sec[2] = 0;
        return;
        case 4:
        if((position.sec[0] & 63) == coords){
            if(position.sec[0] & 64)
                position.isboostavailablesec = true;
            --position.seclinkindex;
            position.sec[0] = position.sec[3];
            position.sec[3] = 0;
            return;
        }
        if(position.sec[1] == coords){
            --position.seclinkindex;
            position.sec[1] = position.sec[3];
            position.sec[3] = 0;
            return;
        }
        if(position.sec[2] == coords){
            --position.seclinkindex;
            position.sec[2] = position.sec[3];
            position.sec[3] = 0;
            return;
        }
        --position.seclinkindex;
        position.sec[3] = 0;
        return;
    }
}

inline void removefirstlink(field &position, const int &coords){
    switch(position.firlinkindex)
    {
        case 1:
        if(position.fir[0] & 64)
            position.isboostavailablefir = true;
        --position.firlinkindex;
        position.fir[0] = 0;
        return;
        case 2:
        if((position.fir[0] & 63) == coords){
            if(position.fir[0] & 64)
                position.isboostavailablefir = true;
            --position.firlinkindex;
            position.fir[0] = position.fir[1];
            position.fir[1] = 0;
            return;
        }
        --position.firlinkindex;
        position.fir[1] = 0;
        return;
        case 3:
        if((position.fir[0] & 63) == coords){
            if(position.fir[0] & 64)
                position.isboostavailablefir = true;
            --position.firlinkindex;
            position.fir[0] = position.fir[2];
            position.fir[2] = 0;
            return;
        }
        if(position.fir[1] == coords){
            --position.firlinkindex;
            position.fir[1] = position.fir[2];
            position.fir[2] = 0;
            return;
        }
        --position.firlinkindex;
        position.fir[2] = 0;
        return;
        case 4:
        if((position.fir[0] & 63) == coords){
            if(position.fir[0] & 64)
                position.isboostavailablefir = true;
            --position.firlinkindex;
            position.fir[0] = position.fir[3];
            position.fir[3] = 0;
            return;
        }
        if(position.fir[1] == coords){
            --position.firlinkindex;
            position.fir[1] = position.fir[3];
            position.fir[3] = 0;
            return;
        }
        if(position.fir[2] == coords){
            --position.firlinkindex;
            position.fir[2] = position.fir[3];
            position.fir[3] = 0;
            return;
        }
        --position.firlinkindex;
        position.fir[3] = 0;
        return;
    }
}

inline void removesecondvirus(field &position, const int &coords){
    switch(position.secvirusindex)
    {
        case 5:
        if(position.sec[4] & 64)
            position.isboostavailablesec = true;
        --position.secvirusindex;
        position.sec[4] = 0;
        return;
        case 6:
        if((position.sec[4] & 63) == coords){
            if(position.sec[4] & 64)
                position.isboostavailablesec = true;
            --position.secvirusindex;
            position.sec[4] = position.sec[5];
            position.sec[5] = 0;
            return;
        }
        --position.secvirusindex;
        position.sec[5] = 0;
        return;
        case 7:
        if((position.sec[4] & 63) == coords){
            if(position.sec[4] & 64)
                position.isboostavailablesec = true;
            --position.secvirusindex;
            position.sec[4] = position.sec[6];
            position.sec[6] = 0;
            return;
        }
        if(position.sec[5] == coords){
            --position.secvirusindex;
            position.sec[5] = position.sec[6];
            position.sec[6] = 0;
            return;
        }
        --position.secvirusindex;
        position.sec[6] = 0;
        return;
        case 8:
        if((position.sec[4] & 63) == coords){
            if(position.sec[4] & 64)
                position.isboostavailablesec = true;
            --position.secvirusindex;
            position.sec[4] = position.sec[7];
            position.sec[7] = 0;
            return;
        }
        if(position.sec[5] == coords){
            --position.secvirusindex;
            position.sec[5] = position.sec[7];
            position.sec[7] = 0;
            return;
        }
        if(position.sec[6] == coords){
            --position.secvirusindex;
            position.sec[6] = position.sec[7];
            position.sec[7] = 0;
            return;
        }
        --position.secvirusindex;
        position.sec[7] = 0;
        return;
    }
}

inline void removefirstvirus(field &position, const int &coords){
    switch(position.firvirusindex)
    {
        case 5:
        if(position.fir[4] & 64)
            position.isboostavailablefir = true;
        --position.firvirusindex;
        position.fir[4] = 0;
        return;
        case 6:
        if((position.fir[4] & 63) == coords){
            if(position.fir[4] & 64)
                position.isboostavailablefir = true;
            --position.firvirusindex;
            position.fir[4] = position.fir[5];
            position.fir[5] = 0;
            return;
        }
        --position.firvirusindex;
        position.fir[5] = 0;
        return;
        case 7:
        if((position.fir[4] & 63) == coords){
            if(position.fir[4] & 64)
                position.isboostavailablefir = true;
            --position.firvirusindex;
            position.fir[4] = position.fir[6];
            position.fir[6] = 0;
            return;
        }
        if(position.fir[5] == coords){
            --position.firvirusindex;
            position.fir[5] = position.fir[6];
            position.fir[6] = 0;
            return;
        }
        --position.firvirusindex;
        position.fir[6] = 0;
        return;
        case 8:
        if((position.fir[4] & 63) == coords){
            if(position.fir[4] & 64)
                position.isboostavailablefir = true;
            --position.firvirusindex;
            position.fir[4] = position.fir[7];
            position.fir[7] = 0;
            return;
        }
        if(position.fir[5] == coords){
            --position.firvirusindex;
            position.fir[5] = position.fir[7];
            position.fir[7] = 0;
            return;
        }
        if(position.fir[6] == coords){
            --position.firvirusindex;
            position.fir[6] = position.fir[7];
            position.fir[7] = 0;
            return;
        }
        --position.firvirusindex;
        position.fir[7] = 0;
        return;
    }
}

vector<field> possiblemoves(const field &position, const bool player){
    vector<field> nplusone;
    nplusone.reserve(40);
    if(player){
        for(int i = 0; i < position.firlinkindex; ++i){
            const int t2 = (position.fir[i] & 63);
            if(t2 == 3 or t2 == 4){
                field temp = position;
                if(position.fir[i] & 64)
                    temp.isboostavailablefir = true;
                --temp.firlinkindex;
                temp.fir[i] = temp.fir[temp.firlinkindex];
                temp.fir[temp.firlinkindex] = 0;
                temp.firl ^= (1ULL << t2);
                ++temp.firlink;
                nplusone.push_back(temp);
            }
        }
        if(position.isboostavailablefir){
            for(int i = 4; i < position.firvirusindex; ++i){
                field temp = position;
                temp.fir[i] |= 64;
                temp.isboostavailablefir = false;
                if(i > 4)
                    swap(temp.fir[i], temp.fir[4]);
                nplusone.push_back(temp);
            }
        }
        const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
        if(firewallsec < 0){
            int startlink = 0, startvirus = 4;
            if(position.isboostavailablefir == false){
                int i;
                if(position.fir[0] & 64){
                    ++startlink;
                    i = 0;
                }
                else{
                    ++startvirus;
                    i = 4;
                }
                const int t = (position.fir[i] & 7), t2 = (position.fir[i] & 63);
                int t4;
                field nmove = position;
                if(i == 0)
                    nmove.firl ^= (1ULL << t2);
                else
                    nmove.firv ^= (1ULL << t2);
                t4 = t2 - 8;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i] -= 8;
                        temp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.fir[i] -= 8;
                        temp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl ^= (1ULL << (t4));
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (1ULL << (t4));
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        t4 -= 8;
                        if(t2 > 15 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] -= 16;
                                temptemp.firl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.fir[i] -= 16;
                                temptemp.firv |= (1ULL << (t4));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl ^= (1ULL << (t4));
                                    removesecondlink(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                                else if(temptemp.firvirus < 3)
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv ^= (1ULL << (t4));
                                    removesecondvirus(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                            }
                            else
                            {
                                nplusone.push_back(temptemp);
                            }
                        }
                        nplusone.push_back(temp);
                    }
                }
                t4 = t2 - 7;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] -= 7;
                        temptemp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.fir[i] -= 7;
                        temptemp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl ^= (1ULL << (t4));
                            removesecondlink(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                        else if(temptemp.firvirus < 3)
                        {
                            ++temptemp.firvirus;
                            temptemp.secv ^= (1ULL << (t4));
                            removesecondvirus(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temptemp);
                    }
                }
                t4 = t2 - 9;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] -= 9;
                        temptemp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.fir[i] -= 9;
                        temptemp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl ^= (1ULL << (t4));
                            removesecondlink(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                        else if(temptemp.firvirus < 3)
                        {
                            ++temptemp.firvirus;
                            temptemp.secv ^= (1ULL << (t4));
                            removesecondvirus(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temptemp);
                    }
                }
                t4 = t2 + 1;
                if(t < 7 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i]++;
                        temp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.fir[i]++;
                        temp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl ^= (1ULL << (t4));
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                            
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (1ULL << (t4));
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        ++t4;
                        if(t < 6 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] += 2;
                                temptemp.firl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.fir[i] += 2;
                                temptemp.firv |= (1ULL << (t4));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl ^= (1ULL << (t4));
                                    removesecondlink(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                                else if(temptemp.firvirus < 3)
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv ^= (1ULL << (t4));
                                    removesecondvirus(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                            }
                            else
                            {
                                nplusone.push_back(temptemp);
                            }
                        }
                        nplusone.push_back(temp);
                    }
                }
                t4 = t2 - 1;
                if(t > 0 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i]--;
                        temp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.fir[i]--;
                        temp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl ^= (1ULL << (t4));
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (1ULL << (t4));
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        --t4;
                        if(t > 1 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] -= 2;
                                temptemp.firl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.fir[i] -= 2;
                                temptemp.firv |= (1ULL << (t4));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl ^= (1ULL << (t4));
                                    removesecondlink(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                                else if(temptemp.firvirus < 3)
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv ^= (1ULL << (t4));
                                    removesecondvirus(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                            }
                            else
                            {
                                nplusone.push_back(temptemp);
                            }
                        }
                        nplusone.push_back(temp);
                    }
                }
                t4 = t2 + 9;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] += 9;
                        temptemp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.fir[i] += 9;
                        temptemp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl ^= (1ULL << (t4));
                            removesecondlink(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                        else if(temptemp.firvirus < 3)
                        {
                            ++temptemp.firvirus;
                            temptemp.secv ^= (1ULL << (t4));
                            removesecondvirus(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temptemp);
                    }
                }
                t4 = t2 + 7;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] += 7;
                        temptemp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.fir[i] += 7;
                        temptemp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl ^= (1ULL << (t4));
                            removesecondlink(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                        else if(temptemp.firvirus < 3)
                        {
                            ++temptemp.firvirus;
                            temptemp.secv ^= (1ULL << (t4));
                            removesecondvirus(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temptemp);
                    }
                }
                t4 = t2 + 8;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i] += 8;
                        temp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.fir[i] += 8;
                        temp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << t4)){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl ^= (1ULL << (t4));
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (1ULL << (t4));
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                        t4 += 8;
                        if(t2 < 48 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] += 16;
                                temptemp.firl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.fir[i] += 16;
                                temptemp.firv |= (1ULL << (t4));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl ^= (1ULL << (t4));
                                    removesecondlink(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                                else if(temptemp.firvirus < 3)
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv ^= (1ULL << (t4));
                                    removesecondvirus(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                            }
                            else
                            {
                                nplusone.push_back(temptemp);
                            }
                        }
                    }
                }
            }
            else
                for(int i = 0; i < position.firlinkindex; ++i){
                    field temp = position;
                    temp.fir[i] |= 64;
                    temp.isboostavailablefir = false;
                    if(i > 0)
                        swap(temp.fir[i], temp.fir[0]);
                    nplusone.push_back(temp);
                }
            for(int i = startvirus; i < position.firvirusindex; ++i){
                const int t = (position.fir[i] & 7), t2 = position.fir[i];
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.firv ^= (1ULL << t2);
                t4 = t2 - 8; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] -= 8;
                    temp.firv |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 += 9; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]++;
                    temp.firv |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]--;
                    temp.firv |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 += 9; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] += 8;
                    temp.firv |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
            }
            for(int i = startlink; i < position.firlinkindex; ++i){
                const int t = (position.fir[i] & 7), t2 = position.fir[i];
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.firl ^= (1ULL << t2);
                t4 = t2 - 8; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] -= 8;
                    temp.firl |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 += 9; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]++;
                    temp.firl |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]--;
                    temp.firl |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 += 9; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] += 8;
                    temp.firl |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
            }
        }
        else
        {
            //todo
        }
        return nplusone;
    }
    else
    {
        for(int i = 0; i < position.seclinkindex; ++i){
            const int t2 = (position.sec[i] & 63);
            if(t2 == 59 or t2 == 60){
                field temp = position;
                if(position.sec[i] & 64)
                    temp.isboostavailablesec = true;
                --temp.seclinkindex;
                temp.sec[i] = temp.sec[temp.seclinkindex];
                temp.sec[temp.seclinkindex] = 0;
                temp.secl ^= (1ULL << t2);
                ++temp.seclink;
                nplusone.push_back(temp);
            }
        }
        if(position.isboostavailablesec){
            for(int i = 4; i < position.secvirusindex; ++i){
                field temp = position;
                temp.sec[i] |= 64;
                temp.isboostavailablesec = false;
                if(i > 4)
                    swap(temp.sec[i], temp.sec[4]);
                nplusone.push_back(temp);
            }
        }
        const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
        if(firewallfir < 0){
            int startlink = 0, startvirus = 4;
            if(position.isboostavailablesec == false){
                int i;
                if(position.sec[0] & 64){
                    ++startlink;
                    i = 0;
                }
                else{
                    ++startvirus;
                    i = 4;
                }
                const int t = (position.sec[i] & 7), t2 = (position.sec[i] & 63), t3 = (position.sec[i] & 56);
                int t4;
                field nmove = position;
                if(i == 0)
                    nmove.secl ^= (1ULL << t2);
                else
                    nmove.secv ^= (1ULL << t2);
                t4 = t2 + 8;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i] += 8;
                        temp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.sec[i] += 8;
                        temp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl ^= (1ULL << (t4));
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (1ULL << (t4));
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        t4 += 8;
                        if(t3 < 48 and (secmask & (1ULL << (t4))) == 0){
                            
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] += 16;
                                temptemp.secl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.sec[i] += 16;
                                temptemp.secv |= (1ULL << (t4));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl ^= (1ULL << (t4));
                                    removefirstlink(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                                else if(temptemp.secvirus < 3)
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv ^= (1ULL << (t4));
                                    removefirstvirus(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                            }
                            else
                            {
                                nplusone.push_back(temptemp);
                            }
                        }
                        nplusone.push_back(temp);
                    }
                }
                t4 = t2 + 9;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] += 9;
                        temptemp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.sec[i] += 9;
                        temptemp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl ^= (1ULL << (t4));
                            removefirstlink(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                        else if(temptemp.secvirus < 3)
                        {
                            ++temptemp.secvirus;
                            temptemp.firv ^= (1ULL << (t4));
                            removefirstvirus(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temptemp);
                    }
                }
                t4 = t2 + 7;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] += 7;
                        temptemp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.sec[i] += 7;
                        temptemp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl ^= (1ULL << (t4));
                            removefirstlink(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                        else if(temptemp.secvirus < 3)
                        {
                            ++temptemp.secvirus;
                            temptemp.firv ^= (1ULL << (t4));
                            removefirstvirus(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temptemp);
                    }
                }
                t4 = t2 + 1;
                if(t < 7 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i]++;
                        temp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.sec[i]++;
                        temp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl ^= (1ULL << (t4));
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (1ULL << (t4));
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        ++t4;
                        if(t < 6 and (secmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] += 2;
                                temptemp.secl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.sec[i] += 2;
                                temptemp.secv |= (1ULL << (t4));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl ^= (1ULL << (t4));
                                    removefirstlink(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                                else if(temptemp.secvirus < 3)
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv ^= (1ULL << (t4));
                                    removefirstvirus(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                            }
                            else
                            {
                                nplusone.push_back(temptemp);
                            }
                        }
                        nplusone.push_back(temp);
                    }
                }
                t4 = t2 - 1;
                if(t > 0 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i]--;
                        temp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.sec[i]--;
                        temp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl ^= (1ULL << (t4));
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (1ULL << (t4));
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        --t4;
                        if(t > 1 and (secmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] -= 2;
                                temptemp.secl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.sec[i] -= 2;
                                temptemp.secv |= (1ULL << (t4));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl ^= (1ULL << (t4));
                                    removefirstlink(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                                else if(temptemp.secvirus < 3)
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv ^= (1ULL << (t4));
                                    removefirstvirus(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                            }
                            else
                            {
                                nplusone.push_back(temptemp);
                            }
                        }
                        nplusone.push_back(temp);
                    }
                }
                t4 = t2 - 7;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] -= 7;
                        temptemp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.sec[i] -= 7;
                        temptemp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl ^= (1ULL << (t4));
                            removefirstlink(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                        else if(temptemp.secvirus < 3)
                        {
                            ++temptemp.secvirus;
                            temptemp.firv ^= (1ULL << (t4));
                            removefirstvirus(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temptemp);
                    }
                }
                t4 = t2 - 9;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] -= 9;
                        temptemp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.sec[i] -= 9;
                        temptemp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl ^= (1ULL << (t4));
                            removefirstlink(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                        else if(temptemp.secvirus < 3)
                        {
                            ++temptemp.secvirus;
                            temptemp.firv ^= (1ULL << (t4));
                            removefirstvirus(temptemp, t4);
                            nplusone.push_back(temptemp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temptemp);
                    }
                }
                t4 = t2 - 8;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i] -= 8;
                        temp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.sec[i] -= 8;
                        temp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl ^= (1ULL << (t4));
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (1ULL << (t4));
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                        t4 -= 8;
                        if(t3 > 15 and (secmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] -= 16;
                                temptemp.secl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.sec[i] -= 16;
                                temptemp.secv |= (1ULL << (t4));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl ^= (1ULL << (t4));
                                    removefirstlink(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                                else if(temptemp.secvirus < 3)
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv ^= (1ULL << (t4));
                                    removefirstvirus(temptemp, t4);
                                    nplusone.push_back(temptemp);
                                }
                            }
                            else
                            {
                                nplusone.push_back(temptemp);
                            }
                        }
                    }
                }
            }
            else
                for(int i = 0; i < position.seclinkindex; ++i){
                    field temp = position;
                    temp.sec[i] |= 64;
                    temp.isboostavailablesec = false;
                    if(i > 0)
                        swap(temp.sec[i], temp.sec[0]);
                    nplusone.push_back(temp);
                }
            for(int i = startvirus; i < position.secvirusindex; ++i){
                const int t = (position.sec[i] & 7), t2 = position.sec[i];
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.secv ^= (1ULL << t2);
                t4 = t2 + 8; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] += 8;
                    temp.secv |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 -= 7; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]++;
                    temp.secv |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]--;
                    temp.secv |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 -= 7; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] -= 8;
                    temp.secv |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
            }
            for(int i = startlink; i < position.seclinkindex; ++i){
                const int t = (position.sec[i] & 7), t2 = position.sec[i];
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.secl ^= (1ULL << t2);
                t4 = t2 + 8; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] += 8;
                    temp.secl |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 -= 7; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]++;
                    temp.secl |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]--;
                    temp.secl |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }
                }
                t4 -= 7; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] -= 8;
                    temp.secl |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            nplusone.push_back(temp);
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            nplusone.push_back(temp);
                        }
                    }
                    else
                    {
                        nplusone.push_back(temp);
                    }

                }
            }
        }
        else
        {
            //todo
        }
    }
    return nplusone;
}

// bool integrity(field pos){
//     uint64_t firl = 0;
//     uint64_t firv = 0;
//     uint64_t secl = 0;
//     uint64_t secv = 0;
//     bool isbf = false;
//     bool isbs = false;
//     int firvirus = 4;
//     int firlink = 4;
//     int secvirus = 4;
//     int seclink = 4;
//     for(int i = 0; i < 8; ++i)
//         if(pos.fir[i] > -1){
//             if(pos.fir[i] & 1024){
//                 firl += (1ULL << (pos.fir[i] & 63));
//                 --seclink;
//             }
//             else{
//                 firv += (1ULL << (pos.fir[i] & 63));
//                 --secvirus;
//             }
//             if(pos.fir[i] & 64){
//                 if(isbf){
//                     cout << "Boost1" << endl;
//                     return false;
//                 }
//                 isbf = true;
//             }
//         }
//     for(int i = 0; i < 8; ++i)
//         if(pos.sec[i] > -1){
//             if(pos.sec[i] & 1024){
//                 secl += (1ULL << (pos.sec[i] & 63));
//                 --firlink;
//             }
//             else{
//                 secv += (1ULL << (pos.sec[i] & 63));
//                 --firvirus;
//             }
//             if(pos.sec[i] & 64){
//                 if(isbs){
//                     cout << "Boost2" << endl;
//                     return false;
//                 }
//                 isbs = true;
//             }
//         }
//     if(firl != pos.firl){ 
//         cout << 1 << endl;
//         return false;
//     }
//     if(firv != pos.firv){ 
//         cout << 2 << endl;
//         return false;
//     }
//     if(secl != pos.secl){ 
//         cout << 3 << endl;
//         return false;
//     }
//     if(secv != pos.secv){ 
//         cout << 4 << endl;
//         return false;
//     }
//     // if(firvirus != pos.firvirus){ 
//     //     cout << 5 << endl;
//     //     return false;
//     // }
//     // if(secvirus != pos.secvirus){ 
//     //     cout << 6 << endl;
//     //     return false;
//     // }
//     // if(firlink != pos.firlink){ 
//     //     cout << "Calculated: " << firlink << endl;
//     //     cout << "Stored: " << pos.firlink << endl;
//     //     cout << 7 << endl;
//     //     return false;
//     // }
//     // if(seclink != pos.seclink){ 
//     //     cout << 8 << endl;
//     //     return false;
//     // }
//     return true;
// }

const int mincachedepth = 2, mincachedepthfull = 2, maxthreads = 50, mindepthformultithreadedsearch = 9;

int minimax(int depth, int alpha, int beta, const bool player, field &position, vector<unordered_map<field, ttentry, field>> &cache, bool &terminate);

inline void minimaxfullfir(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<unordered_map<field, ttentry, field>> &cache, bool &terminate){
    if(depth == 0)
        reschild = position.evaluatefir();
    else
        reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
}

inline void minimaxscoutfir(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<unordered_map<field, ttentry, field>> &cache, bool &terminate){
    if(depth == 0)
        reschild = position.evaluatefir();
    else
    {
        if(beta < MAX){
            reschild = minimax(depth, beta - 1, beta, true, position, cache, terminate);
            if(reschild > alpha and reschild < beta)
                reschild = minimax(depth, alpha, reschild, true, position, cache, terminate);
        }
        else
            reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
    }
}

inline void minimaxfullsec(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<unordered_map<field, ttentry, field>> &cache, bool &terminate){
    if(depth == 0)
        reschild = position.evaluatesec();
    else
        reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
}

inline void minimaxscoutsec(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<unordered_map<field, ttentry, field>> &cache, bool &terminate){
    if(depth == 0)
        reschild = position.evaluatesec();
    else
    {
        if(alpha > MIN and depth > 0){
            reschild = minimax(depth, alpha, alpha + 1, false, position, cache, terminate);
            if(reschild > alpha and reschild < beta)
                reschild = minimax(depth, reschild, beta, false, position, cache, terminate);
        }
        else
            reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
    }
}

int minimax(int depth, int alpha, int beta, const bool player, field &position, vector<unordered_map<field, ttentry, field>> &cache, bool &terminate){
    if(player){
        if(terminate)
            return beta;
        if(depth == 0)
            return position.evaluatefir();
        --depth;
        if(position.firlink > 2){
            const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
            if(position.fir[4] & 64){
                const int cachedpos = (position.fir[4] & 63), t = (cachedpos & 7);
                if(cachedpos < 56){
                    if(position.secl & (1ULL << (cachedpos + 8)))
                        return (16384 * depth);
                    if(cachedpos < 48)
                        if(position.secl & (1ULL << (cachedpos + 16)))
                            if(((position.secv | firmask) & (1ULL << (cachedpos + 8))) == 0)
                                return (16384 * depth);
                    if(t < 7)
                        if(position.secl & (1ULL << (cachedpos + 9)))
                            return (16384 * depth);
                    if(t > 0)
                        if(position.secl & (1ULL << (cachedpos + 7)))
                            return (16384 * depth);
                }
                if(cachedpos > 7){
                    if(position.secl & (1ULL << (cachedpos - 8)))
                        return (16384 * depth);
                    if(cachedpos > 15)
                        if(position.secl & (1ULL << (cachedpos - 16)))
                            if(((position.secv | firmask) & (1ULL << (cachedpos - 8))) == 0)
                                return (16384 * depth);
                    if(t < 7)
                        if(position.secl & (1ULL << (cachedpos - 7)))
                            return (16384 * depth);
                    if(t > 0)
                        if(position.secl & (1ULL << (cachedpos - 9)))
                            return (16384 * depth);
                }
                if(t < 7){
                    if(position.secl & (1ULL << (cachedpos + 1)))
                        return (16384 * depth);
                    if(t < 6)
                        if(position.secl & (1ULL << (cachedpos + 2)))
                            if(((position.secv | firmask) & (1ULL << (cachedpos + 1))) == 0)
                                return (16384 * depth);
                }
                if(t > 0){
                    if(position.secl & (1ULL << (cachedpos - 1)))
                        return (16384 * depth);
                    if(t > 1)
                        if(position.secl & (1ULL << (cachedpos - 2)))
                            if(((position.secv | firmask) & (1ULL << (cachedpos - 1))) == 0)
                                return (16384 * depth);
                }
            }
            else
            {
                const int cachedpos = position.fir[4], t = (cachedpos & 7);
                if(cachedpos < 56)
                    if(position.secl & (1ULL << (cachedpos + 8)))
                        return (16384 * depth);
                if(cachedpos > 7)
                    if(position.secl & (1ULL << (cachedpos - 8)))
                        return (16384 * depth);
                if(t < 7)
                    if(position.secl & (1ULL << (cachedpos + 1)))
                        return (16384 * depth);
                if(t > 0)
                    if(position.secl & (1ULL << (cachedpos - 1)))
                        return (16384 * depth);
            }
            if(position.fir[0] & 64){
                const int cachedpos = (position.fir[0] & 63), t = (cachedpos & 7);
                if(cachedpos == 3 or cachedpos == 4)
                    return (16384 * depth);
                // if(cachedpos == 5 or cachedpos == 4 or cachedpos == 3 or cachedpos == 2 or (cachedpos == 11 and ((secmask | firmask) & (1ULL << 3)) == 0) or (cachedpos == 12 and ((secmask | firmask) & (1ULL << 4)) == 0))
                //     return (16384 * depth);
                if(cachedpos < 56){
                    if(position.secl & (1ULL << (cachedpos + 8)))
                        return (16384 * depth);
                    if(cachedpos < 48)
                        if(position.secl & (1ULL << (cachedpos + 16)))
                            if(((position.secv | firmask) & (1ULL << (cachedpos + 8))) == 0)
                                return (16384 * depth);
                    if(t < 7)
                        if(position.secl & (1ULL << (cachedpos + 9)))
                            return (16384 * depth);
                    if(t > 0)
                        if(position.secl & (1ULL << (cachedpos + 7)))
                            return (16384 * depth);
                }
                if(cachedpos > 7){
                    if(position.secl & (1ULL << (cachedpos - 8)))
                        return (16384 * depth);
                    if(cachedpos > 15)
                        if(position.secl & (1ULL << (cachedpos - 16)))
                            if(((position.secv | firmask) & (1ULL << (cachedpos - 8))) == 0)
                                return (16384 * depth);
                    if(t < 7)
                        if(position.secl & (1ULL << (cachedpos - 7)))
                            return (16384 * depth);
                    if(t > 0)
                        if(position.secl & (1ULL << (cachedpos - 9)))
                            return (16384 * depth);
                }
                if(t < 7){
                    if(position.secl & (1ULL << (cachedpos + 1)))
                        return (16384 * depth);
                    if(t < 6)
                        if(position.secl & (1ULL << (cachedpos + 2)))
                            if(((position.secv | firmask) & (1ULL << (cachedpos + 1))) == 0)
                                return (16384 * depth);
                }
                if(t > 0){
                    if(position.secl & (1ULL << (cachedpos - 1)))
                        return (16384 * depth);
                    if(t > 1)
                        if(position.secl & (1ULL << (cachedpos - 2)))
                            if(((position.secv | firmask) & (1ULL << (cachedpos - 1))) == 0)
                                return (16384 * depth);
                }
            }
            else
            {
                const int cachedpos = position.fir[0], t = (cachedpos & 7);
                if(cachedpos == 3 or cachedpos == 4)
                    return (16384 * depth);
                if(cachedpos < 56)
                    if(position.secl & (1ULL << (cachedpos + 8)))
                        return (16384 * depth);
                if(cachedpos > 7)
                    if(position.secl & (1ULL << (cachedpos - 8)))
                        return (16384 * depth);
                if(t < 7)
                    if(position.secl & (1ULL << (cachedpos + 1)))
                        return (16384 * depth);
                if(t > 0)
                    if(position.secl & (1ULL << (cachedpos - 1)))
                        return (16384 * depth);
            }
            for(int i = 5; i < position.firvirusindex; ++i){
                const int cachedpos = position.fir[i], t = (cachedpos & 7);
                if(cachedpos < 56)
                    if(position.secl & (1ULL << (cachedpos + 8)))
                        return (16384 * depth);
                if(cachedpos > 7)
                    if(position.secl & (1ULL << (cachedpos - 8)))
                        return (16384 * depth);
                if(t < 7)
                    if(position.secl & (1ULL << (cachedpos + 1)))
                        return (16384 * depth);
                if(t > 0)
                    if(position.secl & (1ULL << (cachedpos - 1)))
                        return (16384 * depth);
            }
            for(int i = 1; i < position.firlinkindex; ++i){
                const int cachedpos = position.fir[i], t = (cachedpos & 7);
                if(cachedpos == 3 or cachedpos == 4)
                    return (16384 * depth);
                if(cachedpos < 56)
                    if(position.secl & (1ULL << (cachedpos + 8)))
                        return (16384 * depth);
                if(cachedpos > 7)
                    if(position.secl & (1ULL << (cachedpos - 8)))
                        return (16384 * depth);
                if(t < 7)
                    if(position.secl & (1ULL << (cachedpos + 1)))
                        return (16384 * depth);
                if(t > 0)
                    if(position.secl & (1ULL << (cachedpos - 1)))
                        return (16384 * depth);
            }
        }
        int alphabeg;
        if(depth > mincachedepth){
            alphabeg = alpha;
            auto it = cache[depth].find(position);
            if (it != cache[depth].end()){
                ttentry entry = it->second;
                if(entry.flag & 1){
                    if(entry.score <= alpha) //if current alpha >= cached alpha then the alpha during evaluation wont change, thus we can return the current alpha
                        return alpha;
                    if(entry.flag > 1) //if the cached alpha is exact and it is bigger than the current alpha (because of the condition above) then we can return it
                        return entry.score;
                    beta = min(beta, entry.score);
                    //cached alpha is lower bound
                }
                else
                {
                    if(entry.score > alpha){
                        if(entry.score >= beta)
                            return entry.score;
                        alpha = entry.score;
                    }
                }
            }
        }
        for(int i = 0; i < position.firlinkindex; ++i){
            const int t2 = (position.fir[i] & 63);
            if(t2 == 3 or t2 == 4){
                field temp = position;
                if(position.fir[i] & 64)
                    temp.isboostavailablefir = true;
                --temp.firlinkindex;
                temp.fir[i] = temp.fir[temp.firlinkindex];
                temp.fir[temp.firlinkindex] = 0;
                temp.firl ^= (1ULL << t2);
                ++temp.firlink;
                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                if(reschild > alpha){
                    if(beta <= reschild){
                        if(depth > mincachedepth)
                            cache[depth][position] = {reschild, 0};
                        return reschild;
                    }
                    alpha = reschild;
                }
            }
        }
        if(position.isboostavailablefir){
            for(int i = 4; i < position.firvirusindex; ++i){
                field temp = position;
                temp.fir[i] |= 64;
                temp.isboostavailablefir = false;
                if(i > 4)
                    swap(temp.fir[i], temp.fir[4]);
                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                if(reschild > alpha){
                    if(beta <= reschild){
                        if(depth > mincachedepth)
                            cache[depth][position] = {reschild, 0};
                        return reschild;
                    }
                    alpha = reschild;
                }
            }
        }
        const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
        if(firewallsec < 0){
            int startlink = 0, startvirus = 4;
            if(position.isboostavailablefir == false){
                int i;
                if(position.fir[0] & 64){
                    ++startlink;
                    i = 0;
                }
                else{
                    ++startvirus;
                    i = 4;
                }
                const int t = (position.fir[i] & 7), t2 = (position.fir[i] & 63);
                int t4;
                field nmove = position;
                if(i == 0)
                    nmove.firl ^= (1ULL << t2);
                else
                    nmove.firv ^= (1ULL << t2);
                t4 = t2 - 8;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i] -= 8;
                        temp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.fir[i] -= 8;
                        temp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl ^= (1ULL << (t4));
                            removesecondlink(temp, t4);
                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (1ULL << (t4));
                            removesecondvirus(temp, t4);
                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        t4 -= 8;
                        if(t2 > 15 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] -= 16;
                                temptemp.firl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.fir[i] -= 16;
                                temptemp.firv |= (1ULL << (t4));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl ^= (1ULL << (t4));
                                    removesecondlink(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                    if(reschild > alpha){
                                        if(beta <= reschild){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        alpha = reschild;
                                    }
                                }
                                else if(temptemp.firvirus < 3)
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv ^= (1ULL << (t4));
                                    removesecondvirus(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                    if(reschild > alpha){
                                        if(beta <= reschild){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        alpha = reschild;
                                    }
                                }
                            }
                            else
                            {
                                int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                if(reschild > alpha){
                                    if(beta <= reschild){
                                        if(depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                        }
                        int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 = t2 - 7;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] -= 7;
                        temptemp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.fir[i] -= 7;
                        temptemp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl ^= (1ULL << (t4));
                            removesecondlink(temptemp, t4);
                            int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temptemp.firvirus < 3)
                        {
                            ++temptemp.firvirus;
                            temptemp.secv ^= (1ULL << (t4));
                            removesecondvirus(temptemp, t4);
                            int reschild;
                            if(alpha > MIN and depth > 0){
                                reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(alpha > MIN and depth > 0){
                            reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 = t2 - 9;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] -= 9;
                        temptemp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.fir[i] -= 9;
                        temptemp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl ^= (1ULL << (t4));
                            removesecondlink(temptemp, t4);
                            int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temptemp.firvirus < 3)
                        {
                            ++temptemp.firvirus;
                            temptemp.secv ^= (1ULL << (t4));
                            removesecondvirus(temptemp, t4);
                            int reschild;
                            if(alpha > MIN and depth > 0){
                                reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(alpha > MIN and depth > 0){
                            reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 = t2 + 1;
                if(t < 7 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i]++;
                        temp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.fir[i]++;
                        temp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl ^= (1ULL << (t4));
                            removesecondlink(temp, t4);
                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                            
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (1ULL << (t4));
                            removesecondvirus(temp, t4);
                            int reschild;
                            if(alpha > MIN and depth > 0){
                                reschild = minimax(depth, alpha, alpha + 1, false, temp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, reschild, beta, false, temp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        ++t4;
                        if(t < 6 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] += 2;
                                temptemp.firl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.fir[i] += 2;
                                temptemp.firv |= (1ULL << (t4));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl ^= (1ULL << (t4));
                                    removesecondlink(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                    if(reschild > alpha){
                                        if(beta <= reschild){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        alpha = reschild;
                                    }
                                }
                                else if(temptemp.firvirus < 3)
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv ^= (1ULL << (t4));
                                    removesecondvirus(temptemp, t4);
                                    int reschild;
                                    if(alpha > MIN and depth > 0){
                                        reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                        if(reschild > alpha and reschild < beta)
                                            reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                                    }
                                    else
                                        reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                    if(reschild > alpha){
                                        if(beta <= reschild){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        alpha = reschild;
                                    }
                                }
                            }
                            else
                            {
                                int reschild;
                                if(alpha > MIN and depth > 0){
                                    reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                    if(reschild > alpha and reschild < beta)
                                        reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                                }
                                else
                                    reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                if(reschild > alpha){
                                    if(beta <= reschild){
                                        if(depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                        }
                        int reschild;
                        if(alpha > MIN and depth > 0){
                            reschild = minimax(depth, alpha, alpha + 1, false, temp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, reschild, beta, false, temp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 = t2 - 1;
                if(t > 0 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i]--;
                        temp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.fir[i]--;
                        temp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl ^= (1ULL << (t4));
                            removesecondlink(temp, t4);
                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (1ULL << (t4));
                            removesecondvirus(temp, t4);
                            int reschild;
                            if(alpha > MIN and depth > 0){
                                reschild = minimax(depth, alpha, alpha + 1, false, temp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, reschild, beta, false, temp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        --t4;
                        if(t > 1 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] -= 2;
                                temptemp.firl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.fir[i] -= 2;
                                temptemp.firv |= (1ULL << (t4));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl ^= (1ULL << (t4));
                                    removesecondlink(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                    if(reschild > alpha){
                                        if(beta <= reschild){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        alpha = reschild;
                                    }
                                }
                                else if(temptemp.firvirus < 3)
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv ^= (1ULL << (t4));
                                    removesecondvirus(temptemp, t4);
                                    int reschild;
                                    if(alpha > MIN and depth > 0){
                                        reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                        if(reschild > alpha and reschild < beta)
                                            reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                                    }
                                    else
                                        reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                    if(reschild > alpha){
                                        if(beta <= reschild){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        alpha = reschild;
                                    }
                                }
                            }
                            else
                            {
                                int reschild;
                                if(alpha > MIN and depth > 0){
                                    reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                    if(reschild > alpha and reschild < beta)
                                        reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                                }
                                else
                                    reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                if(reschild > alpha){
                                    if(beta <= reschild){
                                        if(depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                        }
                        int reschild;
                        if(alpha > MIN and depth > 0){
                            reschild = minimax(depth, alpha, alpha + 1, false, temp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, reschild, beta, false, temp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 = t2 + 9;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] += 9;
                        temptemp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.fir[i] += 9;
                        temptemp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl ^= (1ULL << (t4));
                            removesecondlink(temptemp, t4);
                            int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temptemp.firvirus < 3)
                        {
                            ++temptemp.firvirus;
                            temptemp.secv ^= (1ULL << (t4));
                            removesecondvirus(temptemp, t4);
                            int reschild;
                            if(alpha > MIN and depth > 0){
                                reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(alpha > MIN and depth > 0){
                            reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 = t2 + 7;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] += 7;
                        temptemp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.fir[i] += 7;
                        temptemp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl ^= (1ULL << (t4));
                            removesecondlink(temptemp, t4);
                            int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temptemp.firvirus < 3)
                        {
                            ++temptemp.firvirus;
                            temptemp.secv ^= (1ULL << (t4));
                            removesecondvirus(temptemp, t4);
                            int reschild;
                            if(alpha > MIN and depth > 0){
                                reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(alpha > MIN and depth > 0){
                            reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 = t2 + 8;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i] += 8;
                        temp.firl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.fir[i] += 8;
                        temp.firv |= (1ULL << (t4));
                    }
                    if(secmask & (1ULL << t4)){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl ^= (1ULL << (t4));
                            removesecondlink(temp, t4);
                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (1ULL << (t4));
                            removesecondvirus(temp, t4);
                            int reschild;
                            if(alpha > MIN and depth > 0){
                                reschild = minimax(depth, alpha, alpha + 1, false, temp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, reschild, beta, false, temp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(alpha > MIN and depth > 0){
                            reschild = minimax(depth, alpha, alpha + 1, false, temp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, reschild, beta, false, temp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                        t4 += 8;
                        if(t2 < 48 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] += 16;
                                temptemp.firl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.fir[i] += 16;
                                temptemp.firv |= (1ULL << (t4));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl ^= (1ULL << (t4));
                                    removesecondlink(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                    if(reschild > alpha){
                                        if(beta <= reschild){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        alpha = reschild;
                                    }
                                }
                                else if(temptemp.firvirus < 3)
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv ^= (1ULL << (t4));
                                    removesecondvirus(temptemp, t4);
                                    int reschild;
                                    if(alpha > MIN and depth > 0){
                                        reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                        if(reschild > alpha and reschild < beta)
                                            reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                                    }
                                    else
                                        reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                    if(reschild > alpha){
                                        if(beta <= reschild){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        alpha = reschild;
                                    }
                                }
                            }
                            else
                            {
                                int reschild;
                                if(alpha > MIN and depth > 0){
                                    reschild = minimax(depth, alpha, alpha + 1, false, temptemp, cache, terminate);
                                    if(reschild > alpha and reschild < beta)
                                        reschild = minimax(depth, reschild, beta, false, temptemp, cache, terminate);
                                }
                                else
                                    reschild = minimax(depth, alpha, beta, false, temptemp, cache, terminate);
                                if(reschild > alpha){
                                    if(beta <= reschild){
                                        if(depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                        }
                    }
                }
            }
            else
                for(int i = 0; i < position.firlinkindex; ++i){
                    field temp = position;
                    temp.fir[i] |= 64;
                    temp.isboostavailablefir = false;
                    if(i > 0)
                        swap(temp.fir[i], temp.fir[0]);
                    int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                    if(reschild > alpha){
                        if(beta <= reschild){
                            if(depth > mincachedepth)
                                cache[depth][position] = {reschild, 0};
                            return reschild;
                        }
                        alpha = reschild;
                    }
                }
            for(int i = startvirus; i < position.firvirusindex; ++i){
                const int t = (position.fir[i] & 7), t2 = position.fir[i];
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.firv ^= (1ULL << t2);
                t4 = t2 - 8; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] -= 8;
                    temp.firv |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            int reschild;
                            minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            int reschild;
                            minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 += 9; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]++;
                    temp.firv |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            int reschild;
                            minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            int reschild;
                            minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]--;
                    temp.firv |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            int reschild;
                            minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            int reschild;
                            minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 += 9; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] += 8;
                    temp.firv |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            int reschild;
                            minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            int reschild;
                            minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
            }
            for(int i = startlink; i < position.firlinkindex; ++i){
                const int t = (position.fir[i] & 7), t2 = position.fir[i];
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.firl ^= (1ULL << t2);
                t4 = t2 - 8; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] -= 8;
                    temp.firl |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            int reschild;
                            minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            int reschild;
                            minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 += 9; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]++;
                    temp.firl |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            int reschild;
                            minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            int reschild;
                            minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]--;
                    temp.firl |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            int reschild;
                            minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            int reschild;
                            minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
                t4 += 9; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] += 8;
                    temp.firl |= shiftconst;
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl ^= (shiftconst);
                            removesecondlink(temp, t4);
                            int reschild;
                            minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                        else if(temp.firvirus < 3)
                        {
                            ++temp.firvirus;
                            temp.secv ^= (shiftconst);
                            removesecondvirus(temp, t4);
                            int reschild;
                            minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild > alpha){
                                if(beta <= reschild){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild > alpha){
                            if(beta <= reschild){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            alpha = reschild;
                        }
                    }
                }
            }
        }
        else
        {
            //todo
        }
        if(depth > mincachedepthfull and terminate == false)
            cache[depth][position] = {alpha, (alpha > alphabeg) ? uint8_t(3) : uint8_t(1)};
        return alpha;
    }
    else
    {
        if(terminate)
            return alpha;
        if(depth == 0)
            return position.evaluatesec();
        --depth;
        if(position.seclink > 2){
            const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
            if(position.sec[4] & 64){
                const int cachedpos = (position.sec[4] & 63), t = (cachedpos & 7);
                if(cachedpos < 56){
                    if(position.firl & (1ULL << (cachedpos + 8)))
                        return (-16384 * depth);
                    if(cachedpos < 48)
                        if(position.firl & (1ULL << (cachedpos + 16)))
                            if(((position.secv | secmask) & (1ULL << (cachedpos + 8))) == 0)
                                return (-16384 * depth);
                    if(t < 7)
                        if(position.firl & (1ULL << (cachedpos + 9)))
                            return (-16384 * depth);
                    if(t > 0)
                        if(position.firl & (1ULL << (cachedpos + 7)))
                            return (-16384 * depth);
                }
                if(cachedpos > 7){
                    if(position.firl & (1ULL << (cachedpos - 8)))
                        return (-16384 * depth);
                    if(cachedpos > 15)
                        if(position.firl & (1ULL << (cachedpos - 16)))
                            if(((position.secv | secmask) & (1ULL << (cachedpos - 8))) == 0)
                                return (-16384 * depth);
                    if(t < 7)
                        if(position.firl & (1ULL << (cachedpos - 7)))
                            return (-16384 * depth);
                    if(t > 0)
                        if(position.firl & (1ULL << (cachedpos - 9)))
                            return (-16384 * depth);
                }
                if(t < 7){
                    if(position.firl & (1ULL << (cachedpos + 1)))
                        return (-16384 * depth);
                    if(t < 6)
                        if(position.firl & (1ULL << (cachedpos + 2)))
                            if(((position.secv | secmask) & (1ULL << (cachedpos + 1))) == 0)
                                return (-16384 * depth);
                }
                if(t > 0){
                    if(position.firl & (1ULL << (cachedpos - 1)))
                        return (-16384 * depth);
                    if(t > 1)
                        if(position.firl & (1ULL << (cachedpos - 2)))
                            if(((position.secv | secmask) & (1ULL << (cachedpos - 1))) == 0)
                                return (-16384 * depth);
                }
            }
            else
            {
                const int cachedpos = position.sec[4], t = (cachedpos & 7);
                if(cachedpos < 56)
                    if(position.firl & (1ULL << (cachedpos + 8)))
                        return (-16384 * depth);
                if(cachedpos > 7)
                    if(position.firl & (1ULL << (cachedpos - 8)))
                        return (-16384 * depth);
                if(t < 7)
                    if(position.firl & (1ULL << (cachedpos + 1)))
                        return (-16384 * depth);
                if(t > 0)
                    if(position.firl & (1ULL << (cachedpos - 1)))
                        return (-16384 * depth);
            }
            if(position.sec[0] & 64){
                const int cachedpos = (position.sec[0] & 63), t = (cachedpos & 7);
                if(cachedpos == 59 or cachedpos == 60)
                    return (-16384 * depth);
                // if(cachedpos == 58 or cachedpos == 59 or cachedpos == 60 or cachedpos == 61 or (cachedpos == 52 and ((firmask | secmask) & (1ULL << 60)) == 0) or (cachedpos == 51 and ((firmask | secmask) & (1ULL << 61)) == 0))
                //     return (-16384 * depth);
                if(cachedpos < 56){
                    if(position.firl & (1ULL << (cachedpos + 8)))
                        return (-16384 * depth);
                    if(cachedpos < 48)
                        if(position.firl & (1ULL << (cachedpos + 16)))
                            if(((position.secv | secmask) & (1ULL << (cachedpos + 8))) == 0)
                                return (-16384 * depth);
                    if(t < 7)
                        if(position.firl & (1ULL << (cachedpos + 9)))
                            return (-16384 * depth);
                    if(t > 0)
                        if(position.firl & (1ULL << (cachedpos + 7)))
                            return (-16384 * depth);
                }
                if(cachedpos > 7){
                    if(position.firl & (1ULL << (cachedpos - 8)))
                        return (-16384 * depth);
                    if(cachedpos > 15)
                        if(position.firl & (1ULL << (cachedpos - 16)))
                            if(((position.secv | secmask) & (1ULL << (cachedpos - 8))) == 0)
                                return (-16384 * depth);
                    if(t < 7)
                        if(position.firl & (1ULL << (cachedpos - 7)))
                            return (-16384 * depth);
                    if(t > 0)
                        if(position.firl & (1ULL << (cachedpos - 9)))
                            return (-16384 * depth);
                }
                if(t < 7){
                    if(position.firl & (1ULL << (cachedpos + 1)))
                        return (-16384 * depth);
                    if(t < 6)
                        if(position.firl & (1ULL << (cachedpos + 2)))
                            if(((position.secv | secmask) & (1ULL << (cachedpos + 1))) == 0)
                                return (-16384 * depth);
                }
                if(t > 0){
                    if(position.firl & (1ULL << (cachedpos - 1)))
                        return (-16384 * depth);
                    if(t > 1)
                        if(position.firl & (1ULL << (cachedpos - 2)))
                            if(((position.secv | secmask) & (1ULL << (cachedpos - 1))) == 0)
                                return (-16384 * depth);
                }
            }
            else
            {
                const int cachedpos = position.sec[0], t = (cachedpos & 7);
                if(cachedpos == 59 or cachedpos == 60)
                    return (-16384 * depth);
                if(cachedpos < 56)
                    if(position.firl & (1ULL << (cachedpos + 8)))
                        return (-16384 * depth);
                if(cachedpos > 7)
                    if(position.firl & (1ULL << (cachedpos - 8)))
                        return (-16384 * depth);
                if(t < 7)
                    if(position.firl & (1ULL << (cachedpos + 1)))
                        return (-16384 * depth);
                if(t > 0)
                    if(position.firl & (1ULL << (cachedpos - 1)))
                        return (-16384 * depth);
            }
            for(int i = 5; i < position.secvirusindex; ++i){
                const int cachedpos = position.sec[i], t = (cachedpos & 7);
                if(cachedpos < 56)
                    if(position.firl & (1ULL << (cachedpos + 8)))
                        return (-16384 * depth);
                if(cachedpos > 7)
                    if(position.firl & (1ULL << (cachedpos - 8)))
                        return (-16384 * depth);
                if(t < 7)
                    if(position.firl & (1ULL << (cachedpos + 1)))
                        return (-16384 * depth);
                if(t > 0)
                    if(position.firl & (1ULL << (cachedpos - 1)))
                        return (-16384 * depth);
            }
            for(int i = 1; i < position.seclinkindex; ++i){
                const int cachedpos = position.sec[i], t = (cachedpos & 7);
                if(cachedpos == 59 or cachedpos == 60)
                    return (-16384 * depth);
                if(cachedpos < 56)
                    if(position.firl & (1ULL << (cachedpos + 8)))
                        return (-16384 * depth);
                if(cachedpos > 7)
                    if(position.firl & (1ULL << (cachedpos - 8)))
                        return (-16384 * depth);
                if(t < 7)
                    if(position.firl & (1ULL << (cachedpos + 1)))
                        return (-16384 * depth);
                if(t > 0)
                    if(position.firl & (1ULL << (cachedpos - 1)))
                        return (-16384 * depth);
            }
        }
        int betabeg;
        if(depth > mincachedepth){
            betabeg = beta;
            auto it = cache[depth].find(position);
            if (it != cache[depth].end()){
                ttentry entry = it->second;
                if(entry.flag & 1){
                    if(entry.score >= beta) //if current beta <= cached beta then the beta during evaluation wont change, thus we can return the current beta
                        return beta;
                    if(entry.flag > 1) //if the cached beta is exact and it is smaller than the current beta (because of the condition above) then we can return it
                        return entry.score;
                    alpha = max(alpha, entry.score);
                    //cached beta is upper bound
                }
                else
                {
                    if(entry.score < beta){
                        if(entry.score <= alpha)
                            return entry.score;
                        beta = entry.score;
                    }
                }
            }
        }
        for(int i = 0; i < position.seclinkindex; ++i){
            const int t2 = (position.sec[i] & 63);
            if(t2 == 59 or t2 == 60){
                field temp = position;
                if(position.sec[i] & 64)
                    temp.isboostavailablesec = true;
                --temp.seclinkindex;
                temp.sec[i] = temp.sec[temp.seclinkindex];
                temp.sec[temp.seclinkindex] = 0;
                temp.secl ^= (1ULL << t2);
                ++temp.seclink;
                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                if(beta > reschild){
                    if(reschild <= alpha){
                        if(depth > mincachedepth)
                            cache[depth][position] = {reschild, 0};
                        return reschild;
                    }
                    beta = reschild;
                }
            }
        }
        if(position.isboostavailablesec){
            for(int i = 4; i < position.secvirusindex; ++i){
                field temp = position;
                temp.sec[i] |= 64;
                temp.isboostavailablesec = false;
                if(i > 4)
                    swap(temp.sec[i], temp.sec[4]);
                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                if(beta > reschild){
                    if(reschild <= alpha){
                        if(depth > mincachedepth)
                            cache[depth][position] = {reschild, 0};
                        return reschild;
                    }
                    beta = reschild;
                }
            }
        }
        const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
        if(firewallfir < 0){
            int startlink = 0, startvirus = 4;
            if(position.isboostavailablesec == false){
                int i;
                if(position.sec[0] & 64){
                    ++startlink;
                    i = 0;
                }
                else{
                    ++startvirus;
                    i = 4;
                }
                const int t = (position.sec[i] & 7), t2 = (position.sec[i] & 63), t3 = (position.sec[i] & 56);
                int t4;
                field nmove = position;
                if(i == 0)
                    nmove.secl ^= (1ULL << t2);
                else
                    nmove.secv ^= (1ULL << t2);
                t4 = t2 + 8;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i] += 8;
                        temp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.sec[i] += 8;
                        temp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl ^= (1ULL << (t4));
                            removefirstlink(temp, t4);
                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                            if(beta > reschild){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (1ULL << (t4));
                            removefirstvirus(temp, t4);
                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                            if(beta > reschild){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        t4 += 8;
                        if(t3 < 48 and (secmask & (1ULL << (t4))) == 0){
                            
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] += 16;
                                temptemp.secl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.sec[i] += 16;
                                temptemp.secv |= (1ULL << (t4));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl ^= (1ULL << (t4));
                                    removefirstlink(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                    if(beta > reschild){
                                        if(reschild <= alpha){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        beta = reschild;
                                    }
                                }
                                else if(temptemp.secvirus < 3)
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv ^= (1ULL << (t4));
                                    removefirstvirus(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                    if(beta > reschild){
                                        if(reschild <= alpha){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        beta = reschild;
                                    }
                                }
                            }
                            else
                            {
                                int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                if(beta > reschild){
                                    if(reschild <= alpha){
                                        if(depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                        }
                        int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                        if(beta > reschild){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 = t2 + 9;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] += 9;
                        temptemp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.sec[i] += 9;
                        temptemp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl ^= (1ULL << (t4));
                            removefirstlink(temptemp, t4);
                            int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                            if(beta > reschild){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temptemp.secvirus < 3)
                        {
                            ++temptemp.secvirus;
                            temptemp.firv ^= (1ULL << (t4));
                            removefirstvirus(temptemp, t4);
                            int reschild;
                            if(beta < MAX and depth > 0){
                                reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(beta < MAX and depth > 0){
                            reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 = t2 + 7;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] += 7;
                        temptemp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.sec[i] += 7;
                        temptemp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl ^= (1ULL << (t4));
                            removefirstlink(temptemp, t4);
                            int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                            if(beta > reschild){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temptemp.secvirus < 3)
                        {
                            ++temptemp.secvirus;
                            temptemp.firv ^= (1ULL << (t4));
                            removefirstvirus(temptemp, t4);
                            int reschild;
                            if(beta < MAX and depth > 0){
                                reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(beta < MAX and depth > 0){
                            reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 = t2 + 1;
                if(t < 7 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i]++;
                        temp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.sec[i]++;
                        temp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl ^= (1ULL << (t4));
                            removefirstlink(temp, t4);
                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                            if(beta > reschild){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (1ULL << (t4));
                            removefirstvirus(temp, t4);
                            int reschild;
                            if(beta < MAX and depth > 0){
                                reschild = minimax(depth, beta - 1, beta, true, temp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, alpha, reschild, true, temp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        ++t4;
                        if(t < 6 and (secmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] += 2;
                                temptemp.secl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.sec[i] += 2;
                                temptemp.secv |= (1ULL << (t4));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl ^= (1ULL << (t4));
                                    removefirstlink(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                    if(beta > reschild){
                                        if(reschild <= alpha){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        beta = reschild;
                                    }
                                }
                                else if(temptemp.secvirus < 3)
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv ^= (1ULL << (t4));
                                    removefirstvirus(temptemp, t4);
                                    int reschild;
                                    if(beta < MAX and depth > 0){
                                        reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                        if(reschild > alpha and reschild < beta)
                                            reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                                    }
                                    else
                                        reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                    if(reschild < beta){
                                        if(reschild <= alpha){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        beta = reschild;
                                    }
                                }
                            }
                            else
                            {
                                int reschild;
                                if(beta < MAX and depth > 0){
                                    reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                    if(reschild > alpha and reschild < beta)
                                        reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                                }
                                else
                                    reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                if(reschild < beta){
                                    if(reschild <= alpha){
                                        if(depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                        }
                        int reschild;
                        if(beta < MAX and depth > 0){
                            reschild = minimax(depth, beta - 1, beta, true, temp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, alpha, reschild, true, temp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 = t2 - 1;
                if(t > 0 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i]--;
                        temp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.sec[i]--;
                        temp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl ^= (1ULL << (t4));
                            removefirstlink(temp, t4);
                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                            if(beta > reschild){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (1ULL << (t4));
                            removefirstvirus(temp, t4);
                            int reschild;
                            if(beta < MAX and depth > 0){
                                reschild = minimax(depth, beta - 1, beta, true, temp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, alpha, reschild, true, temp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        --t4;
                        if(t > 1 and (secmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] -= 2;
                                temptemp.secl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.sec[i] -= 2;
                                temptemp.secv |= (1ULL << (t4));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl ^= (1ULL << (t4));
                                    removefirstlink(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                    if(beta > reschild){
                                        if(reschild <= alpha){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        beta = reschild;
                                    }
                                }
                                else if(temptemp.secvirus < 3)
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv ^= (1ULL << (t4));
                                    removefirstvirus(temptemp, t4);
                                    int reschild;
                                    if(beta < MAX and depth > 0){
                                        reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                        if(reschild > alpha and reschild < beta)
                                            reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                                    }
                                    else
                                        reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                    if(reschild < beta){
                                        if(reschild <= alpha){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        beta = reschild;
                                    }
                                }
                            }
                            else
                            {
                                int reschild;
                                if(beta < MAX and depth > 0){
                                    reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                    if(reschild > alpha and reschild < beta)
                                        reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                                }
                                else
                                    reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                if(reschild < beta){
                                    if(reschild <= alpha){
                                        if(depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                        }
                        int reschild;
                        if(beta < MAX and depth > 0){
                            reschild = minimax(depth, beta - 1, beta, true, temp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, alpha, reschild, true, temp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 = t2 - 7;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] -= 7;
                        temptemp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.sec[i] -= 7;
                        temptemp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl ^= (1ULL << (t4));
                            removefirstlink(temptemp, t4);
                            int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                            if(beta > reschild){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temptemp.secvirus < 3)
                        {
                            ++temptemp.secvirus;
                            temptemp.firv ^= (1ULL << (t4));
                            removefirstvirus(temptemp, t4);
                            int reschild;
                            if(beta < MAX and depth > 0){
                                reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(beta < MAX and depth > 0){
                            reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 = t2 - 9;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] -= 9;
                        temptemp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temptemp.sec[i] -= 9;
                        temptemp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl ^= (1ULL << (t4));
                            removefirstlink(temptemp, t4);
                            int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                            if(beta > reschild){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temptemp.secvirus < 3)
                        {
                            ++temptemp.secvirus;
                            temptemp.firv ^= (1ULL << (t4));
                            removefirstvirus(temptemp, t4);
                            int reschild;
                            if(beta < MAX and depth > 0){
                                reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(beta < MAX and depth > 0){
                            reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 = t2 - 8;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i] -= 8;
                        temp.secl |= (1ULL << (t4));
                    }
                    else
                    {
                        temp.sec[i] -= 8;
                        temp.secv |= (1ULL << (t4));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl ^= (1ULL << (t4));
                            removefirstlink(temp, t4);
                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                            if(beta > reschild){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (1ULL << (t4));
                            removefirstvirus(temp, t4);
                            int reschild;
                            if(beta < MAX and depth > 0){
                                reschild = minimax(depth, beta - 1, beta, true, temp, cache, terminate);
                                if(reschild > alpha and reschild < beta)
                                    reschild = minimax(depth, alpha, reschild, true, temp, cache, terminate);
                            }
                            else
                                reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        if(beta < MAX and depth > 0){
                            reschild = minimax(depth, beta - 1, beta, true, temp, cache, terminate);
                            if(reschild > alpha and reschild < beta)
                                reschild = minimax(depth, alpha, reschild, true, temp, cache, terminate);
                        }
                        else
                            reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                        t4 -= 8;
                        if(t3 > 15 and (secmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] -= 16;
                                temptemp.secl |= (1ULL << (t4));
                            }
                            else
                            {
                                temptemp.sec[i] -= 16;
                                temptemp.secv |= (1ULL << (t4));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl ^= (1ULL << (t4));
                                    removefirstlink(temptemp, t4);
                                    int reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                    if(beta > reschild){
                                        if(reschild <= alpha){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        beta = reschild;
                                    }
                                }
                                else if(temptemp.secvirus < 3)
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv ^= (1ULL << (t4));
                                    removefirstvirus(temptemp, t4);
                                    int reschild;
                                    if(beta < MAX and depth > 0){
                                        reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                        if(reschild > alpha and reschild < beta)
                                            reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                                    }
                                    else
                                        reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                    if(reschild < beta){
                                        if(reschild <= alpha){
                                            if(depth > mincachedepth)
                                                cache[depth][position] = {reschild, 0};
                                            return reschild;
                                        }
                                        beta = reschild;
                                    }
                                }
                            }
                            else
                            {
                                int reschild;
                                if(beta < MAX and depth > 0){
                                    reschild = minimax(depth, beta - 1, beta, true, temptemp, cache, terminate);
                                    if(reschild > alpha and reschild < beta)
                                        reschild = minimax(depth, alpha, reschild, true, temptemp, cache, terminate);
                                }
                                else
                                    reschild = minimax(depth, alpha, beta, true, temptemp, cache, terminate);
                                if(reschild < beta){
                                    if(reschild <= alpha){
                                        if(depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                        }
                    }
                }
            }
            else
                for(int i = 0; i < position.seclinkindex; ++i){
                    field temp = position;
                    temp.sec[i] |= 64;
                    temp.isboostavailablesec = false;
                    if(i > 0)
                        swap(temp.sec[i], temp.sec[0]);
                    int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                    if(beta > reschild){
                        if(reschild <= alpha){
                            if(depth > mincachedepth)
                                cache[depth][position] = {reschild, 0};
                            return reschild;
                        }
                        beta = reschild;
                    }
                }
            for(int i = startvirus; i < position.secvirusindex; ++i){
                const int t = (position.sec[i] & 7), t2 = position.sec[i];
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.secv ^= (1ULL << t2);
                t4 = t2 + 8; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] += 8;
                    temp.secv |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            int reschild;
                            minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            int reschild;
                            minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 -= 7; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]++;
                    temp.secv |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            int reschild;
                            minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            int reschild;
                            minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]--;
                    temp.secv |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            int reschild;
                            minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            int reschild;
                            minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 -= 7; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] -= 8;
                    temp.secv |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            int reschild;
                            minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            int reschild;
                            minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
            }
            for(int i = startlink; i < position.seclinkindex; ++i){
                const int t = (position.sec[i] & 7), t2 = position.sec[i];
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.secl ^= (1ULL << t2);
                t4 = t2 + 8; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] += 8;
                    temp.secl |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            int reschild;
                            minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            int reschild;
                            minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 -= 7; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]++;
                    temp.secl |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            int reschild;
                            minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            int reschild;
                            minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]--;
                    temp.secl |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            int reschild;
                            minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            int reschild;
                            minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }
                }
                t4 -= 7; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] -= 8;
                    temp.secl |= shiftconst;
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl ^= (shiftconst);
                            removefirstlink(temp, t4);
                            int reschild;
                            minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                        else if(temp.secvirus < 3)
                        {
                            ++temp.secvirus;
                            temp.firv ^= (shiftconst);
                            removefirstvirus(temp, t4);
                            int reschild;
                            minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                            if(reschild < beta){
                                if(reschild <= alpha){
                                    if(depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                    else
                    {
                        int reschild;
                        minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                        if(reschild < beta){
                            if(reschild <= alpha){
                                if(depth > mincachedepth)
                                    cache[depth][position] = {reschild, 0};
                                return reschild;
                            }
                            beta = reschild;
                        }
                    }

                }
            }
        }
        else
        {
            //todo
        }
        if(depth > mincachedepthfull and terminate == false)
            cache[depth][position] = {beta, (beta < betabeg) ? uint8_t(3) : uint8_t(1)};
        return beta;
    }
}

const int multifinish = 2;

int curfreethreads;

mutex mtx;

void displayProgressBar(const double total, const double finished, const string& text) {
    cout << "\33[2K\r" << flush;
    cout << text << " " << int(finished * 100.0/ total) << " %\r" << flush;
}

/*
int minimaxlayer(int depth, int alpha, int beta, const bool player, field &position, vector<unordered_map<field, ttentry, field>> &cache, bool &terminate, int &index){
    if(player){
        vector<field> allmoves = possiblemoves(position, true);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].firlink > 3)
                return (16384 * depth);
        for(; index < allmoves.size(); ++index){
            int childres;
            if(alpha > MIN){
                childres = minimax(depth - 1, alpha, alpha + 1, false, allmoves[index], cache, terminate);
                if(childres > alpha)
                    childres = minimax(depth - 1, childres, beta, false, allmoves[index], cache, terminate);
            }
            else
                childres = minimax(depth - 1, alpha, beta, false, allmoves[index], cache, terminate);
            if(terminate)
                return alpha;
            if(childres > alpha){
                if(beta <= childres)
                    return childres;
                alpha = childres;
            }
        }
        return alpha;
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].seclink > 3)
                return (-16384 * depth);
        for(; index < allmoves.size(); ++index){
            int childres;
            if(beta < MAX){
                childres = minimax(depth - 1, beta - 1, beta, true, allmoves[index], cache, terminate);
                if(childres < beta)
                    childres = minimax(depth - 1, alpha, childres, true, allmoves[index], cache, terminate);
            }
            else
                childres = minimax(depth - 1, alpha, beta, true, allmoves[index], cache, terminate);
            if(terminate)
                return beta;
            if(childres < beta){
                if(childres <= alpha)
                    return childres;
                beta = childres;
            }
        }
        return beta;
    }
}

mutex conc;

int concurrentminimax(int depth, int alpha, int beta, const bool player, field &position){
    cout << "foijwejfoiwejowe" << endl;
    if(player){
        vector<field> allmoves = possiblemoves(position, true);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].firlink > 3)
                return (16384 * depth);
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int tscore;
        bool toterminate = false;
        for(int i = 0; i < allmoves.size(); ++i){
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &tscore, &toterminate]() {
                vector<unordered_map<field, ttentry, field>> newcache(depth);
                if(alpha > MIN){
                    scores[i] = minimax(depth - 1, alpha, alpha + 1, false, allmoves[i], newcache, toterminate);
                    if(scores[i] > alpha)
                        scores[i] = minimax(depth - 1, scores[i], beta, false, allmoves[i], newcache, toterminate);
                }
                else
                    scores[i] = minimax(depth - 1, alpha, beta, false, allmoves[i], newcache, toterminate);
                conc.lock();
                if(beta <= scores[i] and toterminate == false){
                    toterminate = true;
                    tscore = scores[i];
                }
                conc.unlock();
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        if(toterminate)
            return tscore;
        for(int i = 0; i < allmoves.size(); ++i){
            if(scores[i] > alpha){
                alpha = scores[i];
            }
        }
        return alpha;
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].seclink > 3)
                return (-16384 * depth);
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int tscore;
        bool toterminate = false;
        for(int i = 0; i < allmoves.size(); ++i){
			threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &tscore, &toterminate]() {
                vector<unordered_map<field, ttentry, field>> newcache(depth);
                if(beta < MAX){
                    scores[i] = minimax(depth - 1, beta - 1, beta, true, allmoves[i], newcache, toterminate);
                    if(scores[i] < beta)
                        scores[i] = minimax(depth - 1, alpha, scores[i], true, allmoves[i], newcache, toterminate);
                }
                else
                    scores[i] = minimax(depth - 1, alpha, beta, true, allmoves[i], newcache, toterminate);
                conc.lock();
                if(scores[i] <= alpha and toterminate == false){
                    toterminate = true;
                    tscore = scores[i];
                }
                conc.unlock();
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        if(toterminate)
            return tscore;
        for(int i = 0; i < allmoves.size(); ++i){
            if(beta > scores[i]){
                beta = scores[i];
            }
        }
        return beta;
    }
}


int smartminimax(const int depth, int alpha, int beta, const bool player, field &position, vector<unordered_map<field, ttentry, field>> &cache){
    if(player){
        vector<field> allmoves = possiblemoves(position, true);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].firlink > 3)
                return (16384 * depth);
        for(int i = 0; i < allmoves.size(); ++i){
            int availablethreads = -1;
            mtx.lock();
            if(curfreethreads > 0){
                availablethreads = curfreethreads;
                curfreethreads = 0;
            }
            mtx.unlock();
            if(availablethreads > 0){
                ++availablethreads;
                if(allmoves.size() - i < availablethreads)
                    availablethreads = allmoves.size() - i;
                vector<int> scores(availablethreads);
                vector<thread> threads(availablethreads);
                bool toterminate = false;
                int tscore;
                for(int u = 0; u < availablethreads; ++u, ++i){
                    threads[u] = thread([&scores, u, i, &depth, &alpha, &beta, &allmoves, &cache, &toterminate, &tscore]() {
                        if(u == 0){
                            scores[u] = minimax(depth - 1, alpha, beta, false, allmoves[i], cache, toterminate);
                        }
                        else
                        {
                            vector<unordered_map<field, ttentry, field>> newcache = cache;
                            scores[u] = minimax(depth - 1, alpha, beta, false, allmoves[i], newcache, toterminate);
                        }
                        mtx.lock();
                        if(beta <= scores[u] and toterminate == false){
                            toterminate = true;
                            tscore = scores[u];
                        }
                        ++curfreethreads;
                        mtx.unlock();
                    });
                }
                for(int u = 0; u < availablethreads; ++u)
                    threads[u].join();
                if(toterminate)
                    return tscore;
                for(int u = 0; u < availablethreads; ++u){
                    if(scores[u] > alpha){
                        if(beta <= scores[u])
                            return scores[u];
                        alpha = scores[u];
                    }
                }
            }
            else
            {
                bool toterminate = false;
                int reschild = minimax(depth - 1, alpha, beta, false, allmoves[i], cache, toterminate);
                if(reschild > alpha){
                    if(beta <= reschild)
                        return reschild;
                    alpha = reschild;
                }
            }
        }
        return alpha;
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].seclink > 3)
                return (-16384 * depth);
        for(int i = 0; i < allmoves.size();){
            int availablethreads = -1;
            mtx.lock();
            if(curfreethreads > 0){
                availablethreads = curfreethreads;
                curfreethreads = 0;
            }
            mtx.unlock();
            if(availablethreads > 0 and i > 0){
                ++availablethreads;
                if(allmoves.size() - i < availablethreads)
                    availablethreads = allmoves.size() - i;
                vector<int> scores(availablethreads);
                vector<thread> threads(availablethreads);
                bool toterminate = false;
                int tscore;
                for(int u = 0; u < availablethreads; ++u, ++i){
                    threads[u] = thread([&scores, u, i, &depth, &alpha, &beta, &allmoves, &cache, &toterminate, &tscore]() {
                        if(u == 0){
                            scores[u] = minimax(depth - 1, alpha, beta, true, allmoves[i], cache, toterminate);
                        }
                        else
                        {
                            vector<unordered_map<field, ttentry, field>> newcache = cache;
                            scores[u] = minimax(depth - 1, alpha, beta, true, allmoves[i], newcache, toterminate);
                        }
                        mtx.lock();
                        if(scores[u] <= alpha and toterminate == false){
                            toterminate = true;
                            tscore = scores[u];
                        }
                        ++curfreethreads;
                        mtx.unlock();
                    });
                }
                for(int u = 0; u < availablethreads; ++u)
                    threads[u].join();
                if(toterminate)
                    return tscore;
                for(int u = 0; u < availablethreads; ++u){
                    if(scores[u] < beta){
                        if(scores[u] <= alpha)
                            return scores[u];
                        beta = scores[u];
                    }
                }
            }
            else
            {
                bool toterminate = false;
                int reschild = minimax(depth - 1, alpha, beta, true, allmoves[i], cache, toterminate);
                if(reschild < beta){
                    if(reschild <= alpha)
                        return reschild;
                    beta = reschild;
                }
                ++i;
            }
        }
        return beta;
    }
}

int minimaxlayer(const int depth, int alpha, int beta, const bool player, field &position, vector<unordered_map<field, ttentry, field>> &cache, int muldepth){
    if(muldepth == 0)
        return smartminimax(depth, alpha, beta, player, position, cache);
    if(player){
        vector<field> allmoves = possiblemoves(position, true);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].firlink > 3)
                return (16384 * depth);
        for(int i = 0; i < allmoves.size(); ++i){
            int reschild = minimaxlayer(depth - 1, alpha, beta, false, allmoves[i], cache, muldepth - 1);
            if(reschild > alpha){
                if(beta <= reschild)
                    return reschild;
                alpha = reschild;
            }
        }
        return alpha;
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].seclink > 3)
                return (-16384 * depth);
        for(int i = 0; i < allmoves.size(); ++i){
            int reschild = minimaxlayer(depth - 1, alpha, beta, true, allmoves[i], cache, muldepth - 1);
            if(reschild < beta){
                if(reschild <= alpha)
                    return reschild;
                beta = reschild;
            }
        }
        return beta;
    }
}
*/

int cutoffdepth;

int minimaxscout(const int depth, int alpha, int beta, const bool player, field &position){
    if(depth < cutoffdepth){
        bool toterminate = false;
        vector<unordered_map<field, ttentry, field>> newcache(depth, unordered_map<field, ttentry, field>(1024));
        return minimax(depth, alpha, beta, player, position, newcache, toterminate);
    }
    if(player){
        vector<field> allmoves = possiblemoves(position, true);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].firlink > 3)
                return (16384 * depth);
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        for(int i = 0; i < allmoves.size(); ++i){
			threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]() {
                bool toterminate = false;
                vector<unordered_map<field, ttentry, field>> newcache(depth, unordered_map<field, ttentry, field>(1024));
                scores[i] = minimax(depth - 3, alpha, beta, false, allmoves[i], newcache, toterminate);
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2x/3 ");
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        int max = scores[0], index = 0;
        for(int i = 1; i < allmoves.size(); ++i){
            if(scores[i] > max){
                max = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        alpha = minimaxscout(depth - 1, alpha, beta, false, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        bool toterminate = false;
        int tscore; 
        //cout << endl;
        //cout << "D" << depth << " alpha: " << alpha << endl;
        auto start = high_resolution_clock::now();
        for(int i = 0; i < allmoves.size(); ++i){
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished, &toterminate, &tscore]() {
                vector<unordered_map<field, ttentry, field>> newcache(depth, unordered_map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 1, alpha, alpha + 1, false, allmoves[i], newcache, toterminate);
                auto stop = high_resolution_clock::now();
                //cout << "Scout " << depth << " time: " << duration_cast<milliseconds>(stop - start).count();
                if(scores[i] > alpha){
                //    cout << "    >" << endl;
                    scores[i] = minimax(depth - 1, scores[i], beta, false, allmoves[i], newcache, toterminate);
                }
                //else
                //    cout << endl;
                mtx.lock();
                if(beta <= scores[i] and toterminate == false){
                    toterminate = true;
                    tscore = scores[i];
                }
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2x/3 ");
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        //cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        if(toterminate)
            return tscore;
        for(int i = 0; i < allmoves.size(); ++i){
            if(scores[i] > alpha){
                alpha = scores[i];
            }
        }
        return alpha;
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].seclink > 3)
                return (-16384 * depth);
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        for(int i = 0; i < allmoves.size(); ++i){
			threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]() {
                bool toterminate = false;
                vector<unordered_map<field, ttentry, field>> newcache(depth, unordered_map<field, ttentry, field>(1024));
                scores[i] = minimax(depth - 3, alpha, beta, true, allmoves[i], newcache, toterminate);
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2x/3 ");
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        int min = scores[0], index = 0;
        for(int i = 1; i < allmoves.size(); ++i){
            if(scores[i] < min){
                min = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        beta = minimaxscout(depth - 1, alpha, beta, true, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        bool toterminate = false;
        int tscore;
        //cout << endl;
        //cout << "D" << depth << " beta: " << beta << endl;
        auto start = high_resolution_clock::now();
        for(int i = 0; i < allmoves.size(); ++i){
			threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished, &toterminate, &tscore]() {
                vector<unordered_map<field, ttentry, field>> newcache(depth, unordered_map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 1, beta - 1, beta, true, allmoves[i], newcache, toterminate);
                auto stop = high_resolution_clock::now();
                //cout << "Scout " << depth << " time: " << duration_cast<milliseconds>(stop - start).count();
                if(scores[i] < beta){
                //    cout << "    >" << endl;
                    scores[i] = minimax(depth - 1, alpha, scores[i], true, allmoves[i], newcache, toterminate);
                }
                //else
                //    cout << endl;
                mtx.lock();
                if(scores[i] <= alpha and toterminate == false){
                    toterminate = true;
                    tscore = scores[i];
                }
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2x/3 ");
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        //cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        if(toterminate)
            return tscore;
        for(int i = 0; i < allmoves.size(); ++i){
            if(beta > scores[i]){
                beta = scores[i];
            }
        }
        return beta;
    }
}

pair<field, int> minimaxmain(const int depth, int alpha, int beta, const bool player, const field &position){
    cutoffdepth = depth - 5;
    curfreethreads = 0;
    if(player){
        vector<field> allmoves = possiblemoves(position, true);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].firlink > 3)
                return make_pair(allmoves[i], (16384 * depth));
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        auto start = high_resolution_clock::now();
        for(int i = 0; i < allmoves.size(); ++i){
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]() {
                bool toterminate = false;
                vector<unordered_map<field, ttentry, field>> newcache(depth, unordered_map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 3, alpha, beta, false, allmoves[i], newcache, toterminate);
                auto stop = high_resolution_clock::now();
                //cout << "Predict time: " << duration_cast<milliseconds>(stop - start).count() << endl;
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 1/3 ");
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        int max = scores[0], index = 0;
        for(int i = 1; i < allmoves.size(); ++i){
            if(scores[i] > max){
                max = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        //cout << "Prediction time: " << duration_cast<milliseconds>(end - start).count() << endl;
        field bestfield = allmoves[0];
        alpha = minimaxscout(depth - 1, alpha, beta, false, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        //cout << endl;
        //cout << "alpha: " << alpha << endl;
        bool toterminate = false;
        start = high_resolution_clock::now();
        for(int i = 0; i < allmoves.size(); ++i){
			threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished, &toterminate]() {
                vector<unordered_map<field, ttentry, field>> newcache(depth, unordered_map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 1, alpha, alpha + 1, false, allmoves[i], newcache, toterminate);
                auto stop = high_resolution_clock::now();
                //cout << "Scout " << i << " time: " << duration_cast<milliseconds>(stop - start).count();
                if(scores[i] > alpha){
                    //cout << "    >" << endl;
                    scores[i] = minimax(depth - 1, scores[i], beta, false, allmoves[i], newcache, toterminate);
                }
                //else
                //    cout << endl;
                mtx.lock();
                ++curfreethreads;
                // for(int j = 0; j < depth; ++j)
                //     cout << "j:" << j << " " << newcache[j].size() << endl;
                // if(toterminate){
                //     mtx.unlock();
                //     scores[i] = minimaxscout(depth - 1, alpha, beta, false, allmoves[i]);
                // }
                // else if(allmoves.size() - curfreethreads == multifinish and duration_cast<milliseconds>(stop - start).count() > (800 * (1 << (depth - 12))) and depth > 11){
                //     toterminate = true;
                // }
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 3/3 ");
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        end = high_resolution_clock::now();
        //cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        for(int i = 0; i < allmoves.size(); ++i){
            if(scores[i] > alpha){
                alpha = scores[i];
                bestfield = allmoves[i];
            }
        }
        return make_pair(bestfield, alpha);
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].seclink > 3)
                return make_pair(allmoves[i], (-16384 * depth));
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        auto start = high_resolution_clock::now();
        for(int i = 0; i < allmoves.size(); ++i){
			threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]() {
                bool toterminate = false;
                vector<unordered_map<field, ttentry, field>> newcache(depth, unordered_map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 3, alpha, beta, true, allmoves[i], newcache, toterminate);
                auto stop = high_resolution_clock::now();
                //cout << "Predict time: " << duration_cast<milliseconds>(stop - start).count() << endl;
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 1/3 ");
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        int min = scores[0], index = 0;
        for(int i = 1; i < allmoves.size(); ++i){
            if(scores[i] < min){
                min = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        //cout << "Prediction time: " << duration_cast<milliseconds>(end - start).count() << endl;
        field bestfield = allmoves[0];
        beta = minimaxscout(depth - 1, alpha, beta, true, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        //cout << endl;
        //cout << "beta: " << beta << endl;
        bool toterminate = false;
        start = high_resolution_clock::now();
        for(int i = 0; i < allmoves.size(); ++i){
			threads[i] = thread([&scores, i, depth, &alpha, &beta, &allmoves, &finished, &toterminate]() {
                vector<unordered_map<field, ttentry, field>> newcache(depth, unordered_map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 1, beta - 1, beta, true, allmoves[i], newcache, toterminate);
                auto stop = high_resolution_clock::now();
                //cout << "Scout " << i << " time: " << duration_cast<milliseconds>(stop - start).count();
                if(scores[i] < beta){
                    //cout << "    >" << endl;
                    scores[i] = minimax(depth - 1, alpha, scores[i], true, allmoves[i], newcache, toterminate);
                }
                //else
                //    cout << endl;
                mtx.lock();
                ++curfreethreads;
                // if(toterminate){
                //     mtx.unlock();
                //     scores[i] = minimaxscout(depth - 1, alpha, beta, true, allmoves[i]);
                // }
                // else if(allmoves.size() - curfreethreads == multifinish and duration_cast<milliseconds>(stop - start).count() > (800 * (1 << (depth - 12))) and depth > 11){
                //     toterminate = true;
                // }
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 3/3 ");
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        end = high_resolution_clock::now();
        //cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        for(int i = 0; i < allmoves.size(); ++i){
            if(beta > scores[i]){
                beta = scores[i];
                bestfield = allmoves[i];
            }
        }
        return make_pair(bestfield, beta);
    }
}

/*
field aimove(field pos, bool player, int playerseed, int depth){
    Firwinrate = 0.426735
Secwinrate = 0.222245
Firwinrate = 0.426735
Secwinrate = 0.222245
    field positions[70];
    if(player){
        for(int i = 0; i < 70; ++i){
            generatexfield(positions[i], false, indexes[i]);
            generatexfield(positions[i], true, indexes[playerseed]);
        }
        vector<field> allmoves = possiblemoves(pos, true);
        vector<int> allmovesprob(allmoves.size());
        for(int i = 0; i < 70; ++i){
            //cout << "Analyzing: " << i << " / " << 70 << endl;
            pair<field, int> move = minimaxmain(depth, -100000, 100000, true, positions[i]);
            field temp = move.first;
            
            for(int u = 0; u < allmoves.size(); ++u)
                if(allmoves[u].firl == temp.firl and allmoves[u].firv == temp.firv and allmoves[u].fir[0] == temp.fir[0] and allmoves[u].fir[1] == temp.fir[1] and allmoves[u].fir[2] == temp.fir[2] and allmoves[u].fir[3] == temp.fir[3] and allmoves[u].fir[4] == temp.fir[4] and allmoves[u].fir[5] == temp.fir[5] and allmoves[u].fir[6] == temp.fir[6] and allmoves[u].fir[7] == temp.fir[7])
                    ++allmovesprob[u];
        }
        int max = -1, index;
        for(int i = 0; i < allmovesprob.size(); ++i){
            cout << allmovesprob[i] << endl;
            if(allmovesprob[i] > max){
                max = allmovesprob[i];
                index = i;
            }
        }
        return allmoves[index];
    }
    else
    {
        for(int i = 0; i < 70; ++i){
            generatexfield(positions[i], false, indexes[playerseed]);
            generatexfield(positions[i], true, indexes[i]);
        }
        vector<field> allmoves = possiblemoves(pos, false);
        // for(int i = 0; i < allmoves.size(); ++i)
        //     printfield(allmoves[i]);
        vector<int> allmovesprob(allmoves.size());
        for(int i = 0; i < 70; ++i){
            //cout << "Analyzing: " << i << " / " << 70 << endl;
            pair<field, int> move = minimaxmain(depth, -100000, 100000, false, positions[i]);
            field temp = move.first;
            printfield(temp);
            for(int u = 0; u < allmoves.size(); ++u)
                if(allmoves[u].secl == temp.secl and allmoves[u].secv == temp.secv and allmoves[u].sec[0] == temp.sec[0] and allmoves[u].sec[1] == temp.sec[1] and allmoves[u].sec[2] == temp.sec[2] and allmoves[u].sec[3] == temp.sec[3] and allmoves[u].sec[4] == temp.sec[4] and allmoves[u].sec[5] == temp.sec[5] and allmoves[u].sec[6] == temp.sec[6] and allmoves[u].sec[7] == temp.sec[7])
                    ++allmovesprob[u];
        }
        int max = -1, index;
        for(int i = 0; i < allmovesprob.size(); ++i){
            cout << allmovesprob[i] << endl;
            if(allmovesprob[i] > max){
                max = allmovesprob[i];
                index = i;
            }
        }
        return allmoves[index];
    }
}
*/

inline bool isunique(field &toadd, vector<field> &array){
	for(int i = 0; i < array.size(); ++i)
		if(toadd == array[i])
			return false;
	return true;
}

int main(){
    //465344
    //srand(time(NULL));
    // int aiseed = rand() % 70, playerseed = rand() % 70;
    // generatexfield(pos, true, indexes[aiseed]);
    // generatexfield(pos, false, indexes[playerseed]);
    // printfield(pos);
    // for(;;){
    //     pos = aimove(pos, false, aiseed, 9);
    //     cout << pos.isboostavailablefir << endl;
    //     cout << pos.isboostavailablesec << endl;
    //     printfield(pos);
    //     pos = aimove(pos, true, playerseed, 9);
    //     cout << pos.isboostavailablefir << endl;
    //     cout << pos.isboostavailablesec << endl;
    //     printfield(pos);
    // }
    ifstream load("analyze.bin", ios::binary);
    vector<field> todump;
    vector<int> speed;
    vector<bool> pl;
    if(load){
        int size;
        load.read(reinterpret_cast<char*>(&size), sizeof(int));
        for(int i = 0; i < size; ++i){
            field t;
            load.read(reinterpret_cast<char*>(&t), sizeof(field));
            todump.push_back(t);
            int temp;
            load.read(reinterpret_cast<char*>(&temp), sizeof(int));
            speed.push_back(temp);
            bool tempb;
            load.read(reinterpret_cast<char*>(&tempb), sizeof(bool));
            pl.push_back(tempb);
        }
        for(int i = 0; i < size; ++i){
			cout << i << ") " << speed[i] << "  " << pl[i] << endl;
		}
        int sel;
        cin >> sel;
        if(sel > -1 and sel < size){
            auto start = high_resolution_clock::now();
            pair<field, int> move = minimaxmain(14, MIN, MAX, pl[sel], todump[sel]);
            auto end = high_resolution_clock::now();
            cout << "\33[2K\r" << flush;
            cout << "Minimized score: " << move.second << "      " << duration_cast<milliseconds>(end - start).count() << endl;
            return 0;
        }
    }
    load.close();
    field pos;
    generatexfield(pos, true, 15);
    auto start = high_resolution_clock::now();
    size_t checksum = 0;
    for(int i = 0; i < 256; ++i){
        if(__builtin_popcount(i) == 4){
            generatexfield(pos, false, i); 
            auto startit = high_resolution_clock::now();
            pair<field, int> move = minimaxmain(18, MIN, MAX, true, pos);
            auto endit = high_resolution_clock::now();
            cout << "\33[2K\r" << flush;
            cout << move.second << "     " << duration_cast<milliseconds>(endit - startit).count() << endl;
            checksum ^= hash<int>()(move.second);
            //dump << move.second << endl;
        }
    }
    auto end = high_resolution_clock::now();
    cout << duration_cast<milliseconds>(end - start).count() << endl;
    cout << "hash: " << checksum << endl;
    return 0;
    generatexfield(pos, true, indexes[rand() % 70]);
    generatexfield(pos, false, indexes[rand() % 70]);
    printfield(pos);
    cout << endl;
    auto startm = high_resolution_clock::now();
    for(;;){
        auto start = high_resolution_clock::now();
        pair<field, int> move = minimaxmain(14, MIN, MAX, false, pos);
        auto end = high_resolution_clock::now();
        cout << "\33[2K\r" << flush;
        //cout << "Move time: " << duration_cast<milliseconds>(end - start).count() << endl;
        cout << "Minimized score: " << move.second << "      " << duration_cast<milliseconds>(end - start).count() << endl;
        if(isunique(pos, todump)){
			ofstream dump("analyze.bin", ios::binary);
			todump.push_back(pos);
			speed.push_back(duration_cast<milliseconds>(end - start).count());
			pl.push_back(false);
			int size = todump.size();
			dump.write(reinterpret_cast<const char*>(&size), sizeof(int));
			for(int i = 0; i < size; ++i){
				dump.write(reinterpret_cast<const char*>(&todump[i]), sizeof(field));
				dump.write(reinterpret_cast<const char*>(&speed[i]), sizeof(int));
				bool t = pl[i];
				dump.write(reinterpret_cast<const char*>(&t), sizeof(bool));
			}
		}
        pos = move.first; 
        // vector<field> allmoves = possiblemoves(pos, false);
        // for(int i = 0; i < allmoves.size(); ++i){
        //     printfield(allmoves[i]);
        //     cout << i << " ^" << endl;
        // }
        // int ans;
        // cin >> ans;
        // pos = allmoves[ans];
        // cout << pos.firl << endl;
        // cout << pos.firv << endl;
        // cout << pos.secl << endl;
        // cout << pos.secv << endl;
        // cout << "Boost1: " << pos.isboostavailablefir << endl;
        // cout << "Boost2: " << pos.isboostavailablesec << endl;
        // for(int i = 0; i < 8; ++i)
        //     cout << pos.fir[i] << endl;
        // for(int i = 0; i < 8; ++i)
        //     cout << pos.sec[i] << endl;
        printfield(pos);
        cout << endl;
        if(pos.seclink == 4){
            cout << "Player one wins! " << endl;
            printfield(pos);
            break;
        }
        else if(pos.secvirus == 4){
            cout << "Player one loses! " << endl;
            printfield(pos);
            break;
        }
        start = high_resolution_clock::now();
        move = minimaxmain(15, MIN, MAX, true, pos);
        end = high_resolution_clock::now();
        cout << "\33[2K\r" << flush;
        //cout << "Move time: " << duration_cast<milliseconds>(end - start).count() << endl;
        cout << "Maximized score: " << move.second << "      " << duration_cast<milliseconds>(end - start).count() << endl;
        if(isunique(pos, todump)){
			ofstream dump("analyze.bin", ios::binary);
			todump.push_back(pos);
			speed.push_back(duration_cast<milliseconds>(end - start).count());
			pl.push_back(true);
			int size = todump.size();
			dump.write(reinterpret_cast<const char*>(&size), sizeof(int));
			for(int i = 0; i < size; ++i){
				dump.write(reinterpret_cast<const char*>(&todump[i]), sizeof(field));
				dump.write(reinterpret_cast<const char*>(&speed[i]), sizeof(int));
				bool t = pl[i];
				dump.write(reinterpret_cast<const char*>(&t), sizeof(bool));
			}
		}
        pos = move.first;
        // cout << pos.firl << endl;
        // cout << pos.firv << endl;
        // cout << pos.secl << endl;
        // cout << pos.secv << endl;
        // cout << "Boost1: " << pos.isboostavailablefir << endl;*+
        // cout << "Boost2: " << pos.isboostavailablesec << endl;
        // for(int i = 0; i < 8; ++i)
        //     cout << pos.fir[i] << endl;
        // for(int i = 0; i < 8; ++i)
        // //    cout << pos.sec[i] << endl;
        printfield(pos);
        cout << endl;
        if(pos.firlink == 4){
            cout << "Player two wins! " << endl;
            printfield(pos);
            break;
        }
        else if(pos.firvirus == 4){
            cout << "Player two loses! " << endl;
            printfield(pos);
            break;
        }
        //this_thread::sleep_for(chrono::milliseconds(1000));
    }
    auto endm = high_resolution_clock::now();
    cout << duration_cast<milliseconds>(endm - startm).count() << endl;
    // ifstream getdata("evaluations.txt");
    // int firwin = 0, secwin = 0;
    // for(int i = 0; i < 70; ++i){
    //     for(int u = 0; u < 70; ++u){
    //         cout << u << endl;
    //         field pos;
    //         generatexfield(pos, true, indexes[i]);
    //         generatexfield(pos, false, indexes[u]);
    //         vector<field> moves;
    //         for(;;){
    //             pair<field, int> move = minimaxmain(6, -1000000, 1000000, false, pos);
    //             int check;
    //             getdata >> check;
    //             if(check != move.second){
    //                 cout << "Check error1!" << endl;
    //                 cout << "Expected: " << check << endl;
    //                 cout << "Got: " << move.second << endl;
    //                 return 1;
    //             }
    //             pos = move.first;
    //             bool isfound = false;
    //             for(int it = 0; it < moves.size(); ++it)
    //                 if(moves[it] == pos)
    //                 {
    //                     isfound = true;
    //                     break;
    //                 }
    //             if(isfound)
    //                 break;
    //             moves.push_back(pos);
    //             // printfield(pos);
    //             // cout << endl;
    //             if(pos.seclink == 4){
    //                 firwin++;
    //                 break;
    //             }
    //             else if(pos.secvirus == 4){
    //                 secwin++;
    //                 break;
    //             }
    //             move = minimaxmain(6, -1000000, 1000000, true, pos);
    //             getdata >> check;
    //             if(check != move.second){
    //                 cout << "Check error2!" << endl;
    //                 cout << "Expected: " << check << endl;
    //                 cout << "Got: " << move.second << endl;
    //                 return 1;
    //             }
    //             pos = move.first;
    //             for(int it = 0; it < moves.size(); ++it)
    //                 if(moves[it] == pos)
    //                 {
    //                     isfound = true;
    //                     break;
    //                 }
    //             if(isfound)
    //                 break;
    //             moves.push_back(pos);
    //             if(pos.firlink == 4){
    //                 secwin++;
    //                 break;
    //             }
    //             else if(pos.firvirus == 4){
    //                 firwin++;
    //                 break;
    //             }
    //         }
    //     }
    //     cout << "Firwinrate = " << double(firwin) / double(70 + i * 70) << endl;
    //     cout << "Secwinrate = " << double(secwin) / double(70 + i * 70) << endl;
    // }
    // cout << "Firwinrate = " << double(firwin) / 4900.0 << endl;
    // cout << "Secwinrate = " << double(secwin) / 4900.0 << endl;
}
