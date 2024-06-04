#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <thread>

using namespace std;
using namespace chrono;

const int indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

int firewallfir = -1; //0b ycoords(3bits) xcoords(3bits)
int firewallsec = -1; //0b ycoords(3bits) xcoords(3bits)

struct field{
    uint64_t firl = 0;
    uint64_t firv = 0;
    uint64_t secl = 0;
    uint64_t secv = 0;
    // ints are used for simd instructions benefits
    int fir[8] = {-1, -1, -1, -1, -1, -1, -1, -1}; //0b 0 0 0 0 ycoords(3bits) xcoords(3bits)
    int sec[8] = {-1, -1, -1, -1, -1, -1, -1, -1}; //0b 0 0 0 0 ycoords(3bits) xcoords(3bits)
    int boostedfir = -1;
    int boostedsec = -1;
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
        int res = (firlink << 10) - (firvirus << 11) - (seclink << 11);
        for(int i = 0; i < 8; ++i)
            res -= (fir[i] & 56);
        return res;
    }
    int evaluatesec(){
        int res = (secvirus << 11) - (seclink << 10) + (firlink << 11);
        for(int i = 0; i < 8; ++i)
            res -= (sec[i] & 56);
        return res;
    }
    size_t operator()(const field& s) const {
        return hash<uint64_t>()(s.firl) ^ (hash<uint64_t>()(s.firv)) ^ hash<uint64_t>()(s.secl) ^ hash<uint64_t>()(s.secv);
    }
    bool operator==(const field& other) const {
        if(firl != other.firl)
            return false;
        if(firv != other.firv)
            return false;
        if(secl != other.secl)
            return false;
        if(secv != other.secv)
            return false;
        for(int i = 0; i < 8; ++i)
            if(fir[i] != other.fir[i] or sec[i] != other.sec[i])
                return false;
        if(isboostavailablefir != other.isboostavailablefir)
            return false;
        if(isboostavailablesec != other.isboostavailablesec)
            return false;
        if(isswapavailablefir != other.isswapavailablefir)
            return false;
        if(isswapavailablesec != other.isswapavailablesec)
            return false;
        if(ischeckeravailablefir != other.ischeckeravailablefir)
            return false;
        if(ischeckeravailablesec != other.ischeckeravailablesec)
            return false;
        if(isfirewallavailablefir != other.isfirewallavailablefir)
            return false;
        if(isfirewallavailablesec != other.isfirewallavailablesec)
            return false;
        if(firvirus != other.firvirus)
            return false;
        if(firlink != other.firlink)
            return false;
        if(secvirus != other.secvirus)
            return false;
        if(seclink != other.seclink)
            return false;
        return true;
    }
};

void generatexfield(field &togenerate, bool player, int x){
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

vector<field> possiblemoves(field position, bool player){
    vector<field> nplusone;
    nplusone.reserve(100);
    uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
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
                temp.firl = (temp.firl xor (1ULL << t2));
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
            for(int i = 0; i < position.firlinkindex; ++i){
                field temp = position;
                temp.fir[i] |= 64;
                temp.isboostavailablefir = false;
                if(i > 0)
                    swap(temp.fir[i], temp.fir[0]);
                nplusone.push_back(temp);
            }
        }
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
                    nmove.firl = (nmove.firl xor (1ULL << t2));
                else
                    nmove.firv = (nmove.firv xor (1ULL << t2));
                t4 = t2 - 8;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i] -= 8;
                        temp.firl = (temp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.fir[i] -= 8;
                        temp.firv = (temp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
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
                                temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.fir[i] -= 16;
                                temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.seclinkindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                            temptemp.sec[temptemp.seclinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.secvirusindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                            temptemp.sec[temptemp.secvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            nplusone.push_back(temptemp);
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 = t2 - 7;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] -= 7;
                        temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.fir[i] -= 7;
                        temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.seclinkindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                    temptemp.sec[temptemp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.firvirus;
                            temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.secvirusindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                    temptemp.sec[temptemp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temptemp);
                }
                t4 = t2 - 9;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] -= 9;
                        temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.fir[i] -= 9;
                        temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.seclinkindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                    temptemp.sec[temptemp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.firvirus;
                            temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.secvirusindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                    temptemp.sec[temptemp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temptemp);
                }
                t4 = t2 + 1;
                if(t < 7 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i]++;
                        temp.firl = (temp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.fir[i]++;
                        temp.firv = (temp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
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
                                temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.fir[i] += 2;
                                temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.seclinkindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                            temptemp.sec[temptemp.seclinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.secvirusindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                            temptemp.sec[temptemp.secvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            nplusone.push_back(temptemp);
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 = t2 - 1;
                if(t > 0 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i]--;
                        temp.firl = (temp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.fir[i]--;
                        temp.firv = (temp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
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
                                temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.fir[i] -= 2;
                                temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.seclinkindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                            temptemp.sec[temptemp.seclinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.secvirusindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                            temptemp.sec[temptemp.secvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            nplusone.push_back(temptemp);
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 = t2 + 9;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] += 9;
                        temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.fir[i] += 9;
                        temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.seclinkindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                    temptemp.sec[temptemp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.firvirus;
                            temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.secvirusindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                    temptemp.sec[temptemp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temptemp);
                }
                t4 = t2 + 7;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] += 7;
                        temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.fir[i] += 7;
                        temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.seclinkindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                    temptemp.sec[temptemp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.firvirus;
                            temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.secvirusindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                    temptemp.sec[temptemp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temptemp);
                }
                t4 = t2 + 8;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i] += 8;
                        temp.firl = (temp.firl xor (1ULL << t4));
                    }
                    else
                    {
                        temp.fir[i] += 8;
                        temp.firv = (temp.firv xor (1ULL << t4));
                    }
                    if(secmask & (1ULL << t4)){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    else
                    {
                        t4 += 8;
                        if(t2 < 48 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] += 16;
                                temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.fir[i] += 16;
                                temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.seclinkindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                            temptemp.sec[temptemp.seclinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.secvirusindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                            temptemp.sec[temptemp.secvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            nplusone.push_back(temptemp);
                        }
                    }
                    nplusone.push_back(temp);
                }
            }
            for(int i = startvirus; i < position.firvirusindex; ++i){
                const int t = (position.fir[i] & 7), t2 = (position.fir[i] & 63);
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.firv = (nmove.firv xor (1ULL << t2));
                t4 = t2 - 8; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] -= 8;
                    temp.firv = (temp.firv xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 += 9; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]++;
                    temp.firv = (temp.firv xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]--;
                    temp.firv = (temp.firv xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 += 9; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] += 8;
                    temp.firv = (temp.firv xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
            }
            for(int i = startlink; i < position.firlinkindex; ++i){
                const int t = (position.fir[i] & 7), t2 = (position.fir[i] & 63);
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.firl = (nmove.firl xor (1ULL << t2));
                t4 = t2 - 8; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] -= 8;
                    temp.firl = (temp.firl xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l){
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 += 9; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]++;
                    temp.firl = (temp.firl xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                                ++temp.firlink;
                                temp.secl = (temp.secl xor shiftconst);
                                for(int l = 0;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.seclinkindex;
                                        temp.sec[l] = temp.sec[temp.seclinkindex];
                                        temp.sec[temp.seclinkindex] = 0;
                                        break;
                                    }
                            }
                            else
                            {
                                ++temp.firvirus;
                                temp.secv = (temp.secv xor shiftconst);
                                for(int l = 4;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.secvirusindex;
                                        temp.sec[l] = temp.sec[temp.secvirusindex];
                                        temp.sec[temp.secvirusindex] = 0;
                                        break;
                                    }
                            }
                    }
                    nplusone.push_back(temp);
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]--;
                    temp.firl = (temp.firl xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                                ++temp.firlink;
                                temp.secl = (temp.secl xor shiftconst);
                                for(int l = 0;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.seclinkindex;
                                        temp.sec[l] = temp.sec[temp.seclinkindex];
                                        temp.sec[temp.seclinkindex] = 0;
                                        break;
                                    }
                            }
                            else
                            {
                                ++temp.firvirus;
                                temp.secv = (temp.secv xor shiftconst);
                                for(int l = 4;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.secvirusindex;
                                        temp.sec[l] = temp.sec[temp.secvirusindex];
                                        temp.sec[temp.secvirusindex] = 0;
                                        break;
                                    }
                            }
                    }
                    nplusone.push_back(temp);
                }
                t4 += 9; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] += 8;
                    temp.firl = (temp.firl xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                                ++temp.firlink;
                                temp.secl = (temp.secl xor shiftconst);
                                for(int l = 0;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.seclinkindex;
                                        temp.sec[l] = temp.sec[temp.seclinkindex];
                                        temp.sec[temp.seclinkindex] = 0;
                                        break;
                                    }
                            }
                            else
                            {
                                ++temp.firvirus;
                                temp.secv = (temp.secv xor shiftconst);
                                for(int l = 4;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.secvirusindex;
                                        temp.sec[l] = temp.sec[temp.secvirusindex];
                                        temp.sec[temp.secvirusindex] = 0;
                                        break;
                                    }
                            }
                    }
                    nplusone.push_back(temp);
                }
            }
        }
        else
        {
            //todo
        }
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
                temp.secl = (temp.secl xor (1ULL << t2));
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
            for(int i = 0; i < position.seclinkindex; ++i){
                field temp = position;
                temp.sec[i] |= 64;
                temp.isboostavailablesec = false;
                if(i > 0)
                    swap(temp.sec[i], temp.sec[0]);
                nplusone.push_back(temp);
            }
        }
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
                    nmove.secl = (nmove.secl xor (1ULL << t2));
                else
                    nmove.secv = (nmove.secv xor (1ULL << t2));
                t4 = t2 + 8;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i] += 8;
                        temp.secl = (temp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.sec[i] += 8;
                        temp.secv = (temp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
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
                                temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.sec[i] += 16;
                                temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firlinkindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                            temptemp.fir[temptemp.firlinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firvirusindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                            temptemp.fir[temptemp.firvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            nplusone.push_back(temptemp);
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 = t2 + 9;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] += 9;
                        temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.sec[i] += 9;
                        temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firlinkindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                    temptemp.fir[temptemp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.secvirus;
                            temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firvirusindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                    temptemp.fir[temptemp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temptemp);
                }
                t4 = t2 + 7;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] += 7;
                        temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.sec[i] += 7;
                        temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firlinkindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                    temptemp.fir[temptemp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.secvirus;
                            temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firvirusindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                    temptemp.fir[temptemp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temptemp);
                }
                t4 = t2 + 1;
                if(t < 7 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i]++;
                        temp.secl = (temp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.sec[i]++;
                        temp.secv = (temp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
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
                                temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.sec[i] += 2;
                                temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firlinkindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                            temptemp.fir[temptemp.firlinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firvirusindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                            temptemp.fir[temptemp.firvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            nplusone.push_back(temptemp);
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 = t2 - 1;
                if(t > 0 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i]--;
                        temp.secl = (temp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.sec[i]--;
                        temp.secv = (temp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
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
                                temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.sec[i] -= 2;
                                temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firlinkindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                            temptemp.fir[temptemp.firlinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firvirusindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                            temptemp.fir[temptemp.firvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            nplusone.push_back(temptemp);
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 = t2 - 7;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] -= 7;
                        temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.sec[i] -= 7;
                        temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firlinkindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                    temptemp.fir[temptemp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.secvirus;
                            temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firvirusindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                    temptemp.fir[temptemp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temptemp);
                }
                t4 = t2 - 9;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] -= 9;
                        temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.sec[i] -= 9;
                        temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firlinkindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                    temptemp.fir[temptemp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.secvirus;
                            temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firvirusindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                    temptemp.fir[temptemp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temptemp);
                }
                t4 = t2 - 8;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i] -= 8;
                        temp.secl = (temp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.sec[i] -= 8;
                        temp.secv = (temp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    else
                    {
                        t4 -= 8;
                        if(t3 > 15 and (secmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] -= 16;
                                temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.sec[i] -= 16;
                                temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firlinkindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                            temptemp.fir[temptemp.firlinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firvirusindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                            temptemp.fir[temptemp.firvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            nplusone.push_back(temptemp);
                        }
                    }
                    nplusone.push_back(temp);
                }
            }
            for(int i = startvirus; i < position.secvirusindex; ++i){
                const int t = (position.sec[i] & 7), t2 = (position.sec[i] & 63);
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.secv = (nmove.secv xor (1ULL << t2));
                t4 = t2 + 8; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] += 8;
                    temp.secv = (temp.secv xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 -= 7; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]++;
                    temp.secv = (temp.secv xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]--;
                    temp.secv = (temp.secv xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 -= 7; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] -= 8;
                    temp.secv = (temp.secv xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
            }
            for(int i = startlink; i < position.seclinkindex; ++i){
                const int t = (position.sec[i] & 7), t2 = (position.sec[i] & 63);
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.secl = (nmove.secl xor (1ULL << t2));
                t4 = t2 + 8; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] += 8;
                    temp.secl = (temp.secl xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 -= 7; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]++;
                    temp.secl = (temp.secl xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]--;
                    temp.secl = (temp.secl xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
                }
                t4 -= 7; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] -= 8;
                    temp.secl = (temp.secl xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    nplusone.push_back(temp);
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

bool integrity(field pos){
    uint64_t firl = 0;
    uint64_t firv = 0;
    uint64_t secl = 0;
    uint64_t secv = 0;
    bool isbf = false;
    bool isbs = false;
    int firvirus = 4;
    int firlink = 4;
    int secvirus = 4;
    int seclink = 4;
    for(int i = 0; i < 8; ++i)
        if(pos.fir[i] > -1){
            if(pos.fir[i] & 1024){
                firl += (1ULL << (pos.fir[i] & 63));
                --seclink;
            }
            else{
                firv += (1ULL << (pos.fir[i] & 63));
                --secvirus;
            }
            if(pos.fir[i] & 64){
                if(isbf){
                    cout << "Boost1" << endl;
                    return false;
                }
                isbf = true;
            }
        }
    for(int i = 0; i < 8; ++i)
        if(pos.sec[i] > -1){
            if(pos.sec[i] & 1024){
                secl += (1ULL << (pos.sec[i] & 63));
                --firlink;
            }
            else{
                secv += (1ULL << (pos.sec[i] & 63));
                --firvirus;
            }
            if(pos.sec[i] & 64){
                if(isbs){
                    cout << "Boost2" << endl;
                    return false;
                }
                isbs = true;
            }
        }
    if(firl != pos.firl){ 
        cout << 1 << endl;
        return false;
    }
    if(firv != pos.firv){ 
        cout << 2 << endl;
        return false;
    }
    if(secl != pos.secl){ 
        cout << 3 << endl;
        return false;
    }
    if(secv != pos.secv){ 
        cout << 4 << endl;
        return false;
    }
    // if(firvirus != pos.firvirus){ 
    //     cout << 5 << endl;
    //     return false;
    // }
    // if(secvirus != pos.secvirus){ 
    //     cout << 6 << endl;
    //     return false;
    // }
    // if(firlink != pos.firlink){ 
    //     cout << "Calculated: " << firlink << endl;
    //     cout << "Stored: " << pos.firlink << endl;
    //     cout << 7 << endl;
    //     return false;
    // }
    // if(seclink != pos.seclink){ 
    //     cout << 8 << endl;
    //     return false;
    // }
    return true;
}

int minimax(int depth, int alpha, int beta, bool player, field position){
    uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
    if(player){
        if(position.seclink > 3)
            return (-16384 * depth);
        if(position.secvirus > 3)
            return (16384 * depth);
        if(depth == 0)
            return position.evaluatefir();
        --depth;
        for(int i = 0; i < position.firlinkindex; ++i){
            const int t2 = (position.fir[i] & 63);
            if(t2 == 3 or t2 == 4){
                field temp = position;
                if(position.fir[i] & 64)
                    temp.isboostavailablefir = true;
                --temp.firlinkindex;
                temp.fir[i] = temp.fir[temp.firlinkindex];
                temp.fir[temp.firlinkindex] = 0;
                temp.firl = (temp.firl xor (1ULL << t2));
                ++temp.firlink;
                int reschild = minimax(depth, alpha, beta, false, temp);
                if(reschild > alpha){
                    if(beta <= reschild)
                        return reschild;
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
                int reschild = minimax(depth, alpha, beta, false, temp);
                if(reschild > alpha){
                    if(beta <= reschild)
                        return reschild;
                    alpha = reschild;
                }
            }
            for(int i = 0; i < position.firlinkindex; ++i){
                field temp = position;
                temp.fir[i] |= 64;
                temp.isboostavailablefir = false;
                if(i > 0)
                    swap(temp.fir[i], temp.fir[0]);
                int reschild = minimax(depth, alpha, beta, false, temp);
                if(reschild > alpha){
                    if(beta <= reschild)
                        return reschild;
                    alpha = reschild;
                }
            }
        }
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
                    nmove.firl = (nmove.firl xor (1ULL << t2));
                else
                    nmove.firv = (nmove.firv xor (1ULL << t2));
                t4 = t2 - 8;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i] -= 8;
                        temp.firl = (temp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.fir[i] -= 8;
                        temp.firv = (temp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
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
                                temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.fir[i] -= 16;
                                temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.seclinkindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                            temptemp.sec[temptemp.seclinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.secvirusindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                            temptemp.sec[temptemp.secvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            int reschild = minimax(depth, alpha, beta, false, temptemp);
                            if(reschild > alpha){
                                if(beta <= reschild)
                                    return reschild;
                                alpha = reschild;
                            }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 = t2 - 7;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] -= 7;
                        temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.fir[i] -= 7;
                        temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.seclinkindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                    temptemp.sec[temptemp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.firvirus;
                            temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.secvirusindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                    temptemp.sec[temptemp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temptemp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 = t2 - 9;
                if(t2 > 7 and (firmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] -= 9;
                        temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.fir[i] -= 9;
                        temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.seclinkindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                    temptemp.sec[temptemp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.firvirus;
                            temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.secvirusindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                    temptemp.sec[temptemp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temptemp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 = t2 + 1;
                if(t < 7 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i]++;
                        temp.firl = (temp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.fir[i]++;
                        temp.firv = (temp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
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
                                temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.fir[i] += 2;
                                temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.seclinkindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                            temptemp.sec[temptemp.seclinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.secvirusindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                            temptemp.sec[temptemp.secvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            int reschild = minimax(depth, alpha, beta, false, temptemp);
                            if(reschild > alpha){
                                if(beta <= reschild)
                                    return reschild;
                                alpha = reschild;
                            }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 = t2 - 1;
                if(t > 0 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i]--;
                        temp.firl = (temp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.fir[i]--;
                        temp.firv = (temp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
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
                                temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.fir[i] -= 2;
                                temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.seclinkindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                            temptemp.sec[temptemp.seclinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.secvirusindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                            temptemp.sec[temptemp.secvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            int reschild = minimax(depth, alpha, beta, false, temptemp);
                            if(reschild > alpha){
                                if(beta <= reschild)
                                    return reschild;
                                alpha = reschild;
                            }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 = t2 + 9;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] += 9;
                        temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.fir[i] += 9;
                        temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.seclinkindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                    temptemp.sec[temptemp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.firvirus;
                            temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.secvirusindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                    temptemp.sec[temptemp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temptemp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 = t2 + 7;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.fir[i] += 7;
                        temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.fir[i] += 7;
                        temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                    }
                    if(secmask & (1ULL << (t4))){
                        if(temptemp.secl & (1ULL << (t4))){
                            ++temptemp.firlink;
                            temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.seclinkindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                    temptemp.sec[temptemp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.firvirus;
                            temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.sec[l] & 63) == t4){
                                    if(temptemp.sec[l] & 64)
                                        temptemp.isboostavailablesec = true;
                                    --temptemp.secvirusindex;
                                    temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                    temptemp.sec[temptemp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temptemp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 = t2 + 8;
                if(t2 < 56 and (firmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.fir[i] += 8;
                        temp.firl = (temp.firl xor (1ULL << t4));
                    }
                    else
                    {
                        temp.fir[i] += 8;
                        temp.firv = (temp.firv xor (1ULL << t4));
                    }
                    if(secmask & (1ULL << t4)){
                        if(temp.secl & (1ULL << (t4))){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    else
                    {
                        t4 += 8;
                        if(t2 < 48 and (firmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.fir[i] += 16;
                                temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.fir[i] += 16;
                                temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            }
                            if(secmask & (1ULL << (t4))){
                                if(temptemp.secl & (1ULL << (t4))){
                                    ++temptemp.firlink;
                                    temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.seclinkindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.seclinkindex];
                                            temptemp.sec[temptemp.seclinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.firvirus;
                                    temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.sec[l] & 63) == t4){
                                            if(temptemp.sec[l] & 64)
                                                temptemp.isboostavailablesec = true;
                                            --temptemp.secvirusindex;
                                            temptemp.sec[l] = temptemp.sec[temptemp.secvirusindex];
                                            temptemp.sec[temptemp.secvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            int reschild = minimax(depth, alpha, beta, false, temptemp);
                            if(reschild > alpha){
                                if(beta <= reschild)
                                    return reschild;
                                alpha = reschild;
                            }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
            }
            for(int i = startvirus; i < position.firvirusindex; ++i){
                const int t = (position.fir[i] & 7), t2 = (position.fir[i] & 63);
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.firv = (nmove.firv xor (1ULL << t2));
                t4 = t2 - 8; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] -= 8;
                    temp.firv = (temp.firv xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    if(l < temp.seclinkindex)
                                        temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    if(l < temp.secvirusindex)
                                        temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 += 9; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]++;
                    temp.firv = (temp.firv xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    if(l < temp.seclinkindex)
                                        temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    if(l < temp.secvirusindex)
                                        temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, alpha + 1, false, temp);
                    if(reschild > alpha and reschild < beta)
                        reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]--;
                    temp.firv = (temp.firv xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    if(l < temp.seclinkindex)
                                        temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    if(l < temp.secvirusindex)
                                        temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, alpha + 1, false, temp);
                    if(reschild > alpha and reschild < beta)
                        reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 += 9; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] += 8;
                    temp.firv = (temp.firv xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    if(l < temp.seclinkindex)
                                        temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    if(l < temp.secvirusindex)
                                        temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, alpha + 1, false, temp);
                    if(reschild > alpha and reschild < beta)
                        reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
            }
            for(int i = startlink; i < position.firlinkindex; ++i){
                const int t = (position.fir[i] & 7), t2 = (position.fir[i] & 63);
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.firl = (nmove.firl xor (1ULL << t2));
                t4 = t2 - 8; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] -= 8;
                    temp.firl = (temp.firl xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                            ++temp.firlink;
                            temp.secl = (temp.secl xor shiftconst);
                            for(int l = 0;; ++l){
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.seclinkindex;
                                    if(l < temp.seclinkindex)
                                        temp.sec[l] = temp.sec[temp.seclinkindex];
                                    temp.sec[temp.seclinkindex] = 0;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            ++temp.firvirus;
                            temp.secv = (temp.secv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.sec[l] & 63) == t4){
                                    if(temp.sec[l] & 64)
                                        temp.isboostavailablesec = true;
                                    --temp.secvirusindex;
                                    if(l < temp.secvirusindex)
                                        temp.sec[l] = temp.sec[temp.secvirusindex];
                                    temp.sec[temp.secvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 += 9; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]++;
                    temp.firl = (temp.firl xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                                ++temp.firlink;
                                temp.secl = (temp.secl xor shiftconst);
                                for(int l = 0;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.seclinkindex;
                                        if(l < temp.seclinkindex)
                                            temp.sec[l] = temp.sec[temp.seclinkindex];
                                        temp.sec[temp.seclinkindex] = 0;
                                        break;
                                    }
                            }
                            else
                            {
                                ++temp.firvirus;
                                temp.secv = (temp.secv xor shiftconst);
                                for(int l = 4;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.secvirusindex;
                                        if(l < temp.secvirusindex)
                                            temp.sec[l] = temp.sec[temp.secvirusindex];
                                        temp.sec[temp.secvirusindex] = 0;
                                        break;
                                    }
                            }
                    }
                    int reschild = minimax(depth, alpha, alpha + 1, false, temp);
                    if(reschild > alpha and reschild < beta)
                        reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i]--;
                    temp.firl = (temp.firl xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                                ++temp.firlink;
                                temp.secl = (temp.secl xor shiftconst);
                                for(int l = 0;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.seclinkindex;
                                        if(l < temp.seclinkindex)
                                            temp.sec[l] = temp.sec[temp.seclinkindex];
                                        temp.sec[temp.seclinkindex] = 0;
                                        break;
                                    }
                            }
                            else
                            {
                                ++temp.firvirus;
                                temp.secv = (temp.secv xor shiftconst);
                                for(int l = 4;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.secvirusindex;
                                        if(l < temp.secvirusindex)
                                            temp.sec[l] = temp.sec[temp.secvirusindex];
                                        temp.sec[temp.secvirusindex] = 0;
                                        break;
                                    }
                            }
                    }
                    int reschild = minimax(depth, alpha, alpha + 1, false, temp);
                    if(reschild > alpha and reschild < beta)
                        reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
                t4 += 9; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (firmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.fir[i] += 8;
                    temp.firl = (temp.firl xor shiftconst);
                    if(secmask & shiftconst){
                        if(temp.secl & shiftconst){
                                ++temp.firlink;
                                temp.secl = (temp.secl xor shiftconst);
                                for(int l = 0;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.seclinkindex;
                                        if(l < temp.seclinkindex)
                                            temp.sec[l] = temp.sec[temp.seclinkindex];
                                        temp.sec[temp.seclinkindex] = 0;
                                        break;
                                    }
                            }
                            else
                            {
                                ++temp.firvirus;
                                temp.secv = (temp.secv xor shiftconst);
                                for(int l = 4;; ++l)
                                    if((temp.sec[l] & 63) == t4){
                                        if(temp.sec[l] & 64)
                                            temp.isboostavailablesec = true;
                                        --temp.secvirusindex;
                                        if(l < temp.secvirusindex)
                                            temp.sec[l] = temp.sec[temp.secvirusindex];
                                        temp.sec[temp.secvirusindex] = 0;
                                        break;
                                    }
                            }
                    }
                    int reschild = minimax(depth, alpha, alpha + 1, false, temp);
                    if(reschild > alpha and reschild < beta)
                        reschild = minimax(depth, alpha, beta, false, temp);
                    if(reschild > alpha){
                        if(beta <= reschild)
                            return reschild;
                        alpha = reschild;
                    }
                }
            }
        }
        else
        {
            //todo
        }
        return alpha;
    }
    else
    {
        if(position.firvirus > 3)
            return (-16384 * depth);
        if(position.firlink > 3)
            return (16384 * depth);
        if(depth == 0)
            return position.evaluatesec();
        --depth;
        for(int i = 0; i < position.seclinkindex; ++i){
            const int t2 = (position.sec[i] & 63);
            if(t2 == 59 or t2 == 60){
                field temp = position;
                if(position.sec[i] & 64)
                    temp.isboostavailablesec = true;
                --temp.seclinkindex;
                temp.sec[i] = temp.sec[temp.seclinkindex];
                temp.sec[temp.seclinkindex] = 0;
                temp.secl = (temp.secl xor (1ULL << t2));
                ++temp.seclink;
                int reschild = minimax(depth, alpha, beta, true, temp);
                if(beta > reschild){
                    if(reschild <= alpha)
                        return reschild;
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
                int reschild = minimax(depth, alpha, beta, true, temp);
                if(beta > reschild){
                    if(reschild <= alpha)
                        return reschild;
                    beta = reschild;
                }
            }
            for(int i = 0; i < position.seclinkindex; ++i){
                field temp = position;
                temp.sec[i] |= 64;
                temp.isboostavailablesec = false;
                if(i > 0)
                    swap(temp.sec[i], temp.sec[0]);
                int reschild = minimax(depth, alpha, beta, true, temp);
                if(beta > reschild){
                    if(reschild <= alpha)
                        return reschild;
                    beta = reschild;
                }
            }
        }
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
                    nmove.secl = (nmove.secl xor (1ULL << t2));
                else
                    nmove.secv = (nmove.secv xor (1ULL << t2));
                t4 = t2 + 8;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i] += 8;
                        temp.secl = (temp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.sec[i] += 8;
                        temp.secv = (temp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
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
                                temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.sec[i] += 16;
                                temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firlinkindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                            temptemp.fir[temptemp.firlinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firvirusindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                            temptemp.fir[temptemp.firvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            int reschild = minimax(depth, alpha, beta, true, temptemp);
                            if(beta > reschild){
                                if(reschild <= alpha)
                                    return reschild;
                                beta = reschild;
                            }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 = t2 + 9;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] += 9;
                        temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.sec[i] += 9;
                        temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firlinkindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                    temptemp.fir[temptemp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.secvirus;
                            temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firvirusindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                    temptemp.fir[temptemp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temptemp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 = t2 + 7;
                if(t3 < 56 and (secmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] += 7;
                        temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.sec[i] += 7;
                        temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firlinkindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                    temptemp.fir[temptemp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.secvirus;
                            temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firvirusindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                    temptemp.fir[temptemp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temptemp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 = t2 + 1;
                if(t < 7 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i]++;
                        temp.secl = (temp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.sec[i]++;
                        temp.secv = (temp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
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
                                temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.sec[i] += 2;
                                temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firlinkindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                            temptemp.fir[temptemp.firlinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firvirusindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                            temptemp.fir[temptemp.firvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            int reschild = minimax(depth, alpha, beta, true, temptemp);
                            if(beta > reschild){
                                if(reschild <= alpha)
                                    return reschild;
                                beta = reschild;
                            }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 = t2 - 1;
                if(t > 0 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i]--;
                        temp.secl = (temp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.sec[i]--;
                        temp.secv = (temp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
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
                                temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.sec[i] -= 2;
                                temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firlinkindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                            temptemp.fir[temptemp.firlinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firvirusindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                            temptemp.fir[temptemp.firvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            int reschild = minimax(depth, alpha, beta, true, temptemp);
                            if(beta > reschild){
                                if(reschild <= alpha)
                                    return reschild;
                                beta = reschild;
                            }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 = t2 - 7;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0 and t < 7){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] -= 7;
                        temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.sec[i] -= 7;
                        temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firlinkindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                    temptemp.fir[temptemp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.secvirus;
                            temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firvirusindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                    temptemp.fir[temptemp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temptemp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 = t2 - 9;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0 and t > 0){
                    field temptemp = nmove;
                    if(i == 0){
                        temptemp.sec[i] -= 9;
                        temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temptemp.sec[i] -= 9;
                        temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temptemp.firl & (1ULL << (t4))){
                            ++temptemp.seclink;
                            temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firlinkindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                    temptemp.fir[temptemp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temptemp.secvirus;
                            temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temptemp.fir[l] & 63) == t4){
                                    if(temptemp.fir[l] & 64)
                                        temptemp.isboostavailablefir = true;
                                    --temptemp.firvirusindex;
                                    temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                    temptemp.fir[temptemp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temptemp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 = t2 - 8;
                if(t3 > 7 and (secmask & (1ULL << (t4))) == 0){
                    field temp = nmove;
                    if(i == 0){
                        temp.sec[i] -= 8;
                        temp.secl = (temp.secl xor (1ULL << (t4)));
                    }
                    else
                    {
                        temp.sec[i] -= 8;
                        temp.secv = (temp.secv xor (1ULL << (t4)));
                    }
                    if(firmask & (1ULL << (t4))){
                        if(temp.firl & (1ULL << (t4))){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor (1ULL << (t4)));
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor (1ULL << (t4)));
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    else
                    {
                        t4 -= 8;
                        if(t3 > 15 and (secmask & (1ULL << (t4))) == 0){
                            field temptemp = nmove;
                            if(i == 0){
                                temptemp.sec[i] -= 16;
                                temptemp.secl = (temptemp.secl xor (1ULL << (t4)));
                            }
                            else
                            {
                                temptemp.sec[i] -= 16;
                                temptemp.secv = (temptemp.secv xor (1ULL << (t4)));
                            }
                            if(firmask & (1ULL << (t4))){
                                if(temptemp.firl & (1ULL << (t4))){
                                    ++temptemp.seclink;
                                    temptemp.firl = (temptemp.firl xor (1ULL << (t4)));
                                    for(int l = 0;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firlinkindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firlinkindex];
                                            temptemp.fir[temptemp.firlinkindex] = 0;
                                            break;
                                        }
                                }
                                else
                                {
                                    ++temptemp.secvirus;
                                    temptemp.firv = (temptemp.firv xor (1ULL << (t4)));
                                    for(int l = 4;; ++l)
                                        if((temptemp.fir[l] & 63) == t4){
                                            if(temptemp.fir[l] & 64)
                                                temptemp.isboostavailablefir = true;
                                            --temptemp.firvirusindex;
                                            temptemp.fir[l] = temptemp.fir[temptemp.firvirusindex];
                                            temptemp.fir[temptemp.firvirusindex] = 0;
                                            break;
                                        }
                                }
                            }
                            int reschild = minimax(depth, alpha, beta, true, temptemp);
                            if(beta > reschild){
                                if(reschild <= alpha)
                                    return reschild;
                                beta = reschild;
                            }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
            }
            for(int i = startvirus; i < position.secvirusindex; ++i){
                const int t = (position.sec[i] & 7), t2 = (position.sec[i] & 63);
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.secv = (nmove.secv xor (1ULL << t2));
                t4 = t2 + 8; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] += 8;
                    temp.secv = (temp.secv xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 -= 7; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]++;
                    temp.secv = (temp.secv xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]--;
                    temp.secv = (temp.secv xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 -= 7; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] -= 8;
                    temp.secv = (temp.secv xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
            }
            for(int i = startlink; i < position.seclinkindex; ++i){
                const int t = (position.sec[i] & 7), t2 = (position.sec[i] & 63);
                int t4;
                uint64_t shiftconst;
                field nmove = position;
                nmove.secl = (nmove.secl xor (1ULL << t2));
                t4 = t2 + 8; //t2 + 8
                shiftconst = (1ULL << t4);
                if(t2 < 56 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] += 8;
                    temp.secl = (temp.secl xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 -= 7; //t2 + 1
                shiftconst = (1ULL << t4);
                if(t < 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]++;
                    temp.secl = (temp.secl xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 -= 2; //t2 - 1
                shiftconst = (1ULL << t4);
                if(t > 0 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i]--;
                    temp.secl = (temp.secl xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
                t4 -= 7; //t2 - 8
                shiftconst = (1ULL << t4);
                if(t2 > 7 and (secmask & shiftconst) == 0){
                    field temp = nmove;
                    temp.sec[i] -= 8;
                    temp.secl = (temp.secl xor shiftconst);
                    if(firmask & shiftconst){
                        if(temp.firl & shiftconst){
                            ++temp.seclink;
                            temp.firl = (temp.firl xor shiftconst);
                            for(int l = 0;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firlinkindex;
                                    temp.fir[l] = temp.fir[temp.firlinkindex];
                                    temp.fir[temp.firlinkindex] = 0;
                                    break;
                                }
                        }
                        else
                        {
                            ++temp.secvirus;
                            temp.firv = (temp.firv xor shiftconst);
                            for(int l = 4;; ++l)
                                if((temp.fir[l] & 63) == t4){
                                    if(temp.fir[l] & 64)
                                        temp.isboostavailablefir = true;
                                    --temp.firvirusindex;
                                    temp.fir[l] = temp.fir[temp.firvirusindex];
                                    temp.fir[temp.firvirusindex] = 0;
                                    break;
                                }
                        }
                    }
                    int reschild = minimax(depth, alpha, beta, true, temp);
                    if(beta > reschild){
                        if(reschild <= alpha)
                            return reschild;
                        beta = reschild;
                    }
                }
            }
        }
        else
        {
            //todo
        }
        return beta;
    }
}

int minimaxscout(int depth, int alpha, int beta, bool player, field position){
    if(player){
        vector<field> allmoves = possiblemoves(position, true);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].firlink > 3)
                return (16384 * depth);
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        for(int i = 0; i < allmoves.size(); ++i){
            threads[i] = thread([&scores, i, depth, alpha, beta, allmoves]() {
                scores[i] = minimax(depth - 1, alpha, beta, false, allmoves[i]);
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        field bestfield = position;
        for(int i = 0; i < allmoves.size(); ++i){
            if(scores[i] > alpha){
                alpha = scores[i];
                bestfield = allmoves[i];
            }
        }
        return alpha;
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].seclink > 3)
                return (-16384 * depth);;
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        for(int i = 0; i < allmoves.size(); ++i){
            threads[i] = thread([&scores, i, depth, alpha, beta, allmoves]() {
                scores[i] = minimax(depth - 1, alpha, beta, true, allmoves[i]);
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        field bestfield = position;
        for(int i = 0; i < allmoves.size(); ++i){
            if(beta > scores[i]){
                beta = scores[i];
                bestfield = allmoves[i];
            }
        }
        return beta;
    }
}

bool cmp(int fir, int sec, bool bv){
	if(bv)
		return fir < sec;
	return sec > fir;
}

void sorter(vector<field> &positions, vector<int> &scores, bool bv){
	for(int i = 0; i < scores.size() - 1; ++i){
		int t = -1;
		for(int u = 0; u < scores.size() - 1 - i; ++u){
			if(cmp(scores[u], scores[u + 1], bv)){
				t = scores[u];
				scores[u] = scores[u + 1];
				scores[u + 1] = t;
				swap(positions[u], positions[u + 1]);
			}	
		}
		if(t == -1)
			return;
	}
}

pair<field, int> minimaxmain(int depth, int alpha, int beta, bool player, field position){
    if(player){
        vector<field> allmoves = possiblemoves(position, true);
        for(int i = 0; i < allmoves.size(); ++i)
            if(allmoves[i].firlink > 3)
                return make_pair(allmoves[i], 100000);
        //auto start = high_resolution_clock::now();
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        for(int i = 0; i < allmoves.size(); ++i){
            threads[i] = thread([&scores, depth, i, alpha, beta, allmoves]() {
                scores[i] = minimax(depth - 3, alpha, beta, false, allmoves[i]);
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        sorter(allmoves, scores, true);
        //auto end = high_resolution_clock::now();
        //cout << "Prediction time: " << duration_cast<milliseconds>(end - start).count() << endl;
        field bestfield = allmoves[0];
        //start = high_resolution_clock::now();
        alpha = minimaxscout(depth - 1, alpha, beta, false, allmoves[0]);
        //end = high_resolution_clock::now();
        //cout << "Predicted alpha time: " << duration_cast<milliseconds>(end - start).count() << endl;
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        for(int i = 0; i < allmoves.size(); ++i){
            threads[i] = thread([&scores, i, depth, alpha, beta, allmoves]() {
                scores[i] = minimax(depth - 1, alpha, beta, false, allmoves[i]);
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
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
                return make_pair(allmoves[i], -100000);
        //auto start = high_resolution_clock::now();
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        for(int i = 0; i < allmoves.size(); ++i){
            threads[i] = thread([&scores, depth, i, alpha, beta, allmoves]() {
                scores[i] = minimax(depth - 3, alpha, beta, true, allmoves[i]);
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        sorter(allmoves, scores, false);
        //auto end = high_resolution_clock::now();
        //cout << "Prediction time: " << duration_cast<milliseconds>(end - start).count() << endl;
        field bestfield = allmoves[0];
        //start = high_resolution_clock::now();
        beta = minimaxscout(depth - 1, alpha, beta, true, allmoves[0]);
        //end = high_resolution_clock::now();
        //cout << "Predicted beta time: " << duration_cast<milliseconds>(end - start).count() << endl;
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        for(int i = 0; i < allmoves.size(); ++i){
            threads[i] = thread([&scores, i, depth, alpha, beta, allmoves]() {
                scores[i] = minimax(depth - 1, alpha, beta, true, allmoves[i]);
            });
        }
        for(int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        for(int i = 0; i < allmoves.size(); ++i){
            if(beta > scores[i]){
                beta = scores[i];
                bestfield = allmoves[i];
            }
        }
        return make_pair(bestfield, beta);
    }
}

field aimove(field pos, bool player, int playerseed, int depth){
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

int main(){
    //465344
    //srand(time(NULL));
    field pos;
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
    //ofstream dump("results.txt");
    generatexfield(pos, true, 15);
    auto start = high_resolution_clock::now();
    for(int i = 0; i < 256; ++i){
        if(__builtin_popcount(i) == 4){
            generatexfield(pos, false, i);
            pair<field, int> move = minimaxmain(12, -100000, 100000, true, pos);
            cout << move.second << endl;
            //dump << move.second << endl;
        }
    }
    auto end = high_resolution_clock::now();
    cout << duration_cast<milliseconds>(end - start).count() << endl;
    // generatexfield(pos, true, rand() % 70);
    // generatexfield(pos, false, rand() % 70);
    // printfield(pos);
    // cout << endl;
    // for(;;){
    //     auto start = high_resolution_clock::now();
    //     pair<field, int> move = minimaxmain(12, -1000000, 1000000, false, pos);
    //     auto end = high_resolution_clock::now();
    //     cout << "Move time: " << duration_cast<milliseconds>(end - start).count() << endl;
    //     cout << "Minimized score: " << move.second << endl;
    //     pos = move.first; 
    //     cout << pos.firl << endl;
    //     cout << pos.firv << endl;
    //     cout << pos.secl << endl;
    //     cout << pos.secv << endl;
    //     cout << "Boost1: " << pos.isboostavailablefir << endl;
    //     cout << "Boost2: " << pos.isboostavailablesec << endl;
    //     for(int i = 0; i < 8; ++i)
    //         cout << pos.fir[i] << endl;
    //     for(int i = 0; i < 8; ++i)
    //         cout << pos.sec[i] << endl;
    //     printfield(pos);
    //     cout << endl;
    //     if(pos.seclink == 4){
    //         cout << "Player one wins! " << endl;
    //         printfield(pos);
    //         break;
    //     }
    //     else if(pos.secvirus == 4){
    //         cout << "Player one loses! " << endl;
    //         printfield(pos);
    //         break;
    //     }
    //     start = high_resolution_clock::now();
    //     move = minimaxmain(12, -1000000, 1000000, true, pos);
    //     end = high_resolution_clock::now();
    //     cout << "Move time: " << duration_cast<milliseconds>(end - start).count() << endl;
    //     cout << "Maximized score: " << move.second << endl;
    //     pos = move.first;
    //     cout << pos.firl << endl;
    //     cout << pos.firv << endl;
    //     cout << pos.secl << endl;
    //     cout << pos.secv << endl;
    //     cout << "Boost1: " << pos.isboostavailablefir << endl;
    //     cout << "Boost2: " << pos.isboostavailablesec << endl;
    //     for(int i = 0; i < 8; ++i)
    //         cout << pos.fir[i] << endl;
    //     for(int i = 0; i < 8; ++i)
    //         cout << pos.sec[i] << endl;
    //     printfield(pos);
    //     cout << endl;
    //     if(pos.firlink == 4){
    //         cout << "Player two wins! " << endl;
    //         printfield(pos);
    //         break;
    //     }
    //     else if(pos.firvirus == 4){
    //         cout << "Player two loses! " << endl;
    //         printfield(pos);
    //         break;
    //     }
    //     //this_thread::sleep_for(chrono::milliseconds(1000));
    // }
}
