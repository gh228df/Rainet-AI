#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <ankerl/unordered_dense.h>
#include <thread>
#include <mutex>

using namespace std;
using namespace chrono;

const int indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

int firewallfir = -1; // 0b ycoords(3bits) xcoords(3bits)
int firewallsec = -1; // 0b ycoords(3bits) xcoords(3bits)

#define MIN -1000000
#define MAX 1000000

struct ttentry
{
    int score;
    int flag;
};

struct field
{
    uint64_t firl;
    uint64_t firv;
    uint64_t secl;
    uint64_t secv;
    // ints are used for simd instructions benefits
    int fir[8]; // 0b 0 0 0 0 ycoords(3bits) xcoords(3bits)
    int sec[8]; // 0b 0 0 0 0 ycoords(3bits) xcoords(3bits)
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
    int evaluatefir()
    {
        int res = (firlink << 10) - (firvirus << 11) - (seclink << 11) + (secvirus << 10);
        for (int i = 0; i < 8; ++i)
            res -= (fir[i] & 56) - 56;
        return res;
    }
    int evaluatesec()
    {
        int res = (secvirus << 11) - (seclink << 10) + (firlink << 11) - (firvirus << 10);
        for (int i = 0; i < 8; ++i)
            res -= (sec[i] & 56);
        return res;
    }
    size_t operator()(const field &s) const
    {
        return (s.firl | s.firv | s.secl | s.secv) ^ ((fir[0] << 7) | (fir[4] << 14) | (sec[0] << 21) | sec[4]);
    }
    bool operator==(const field &other) const
    {
        if (firl == other.firl and firv == other.firv and
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
            seclink == other.seclink)
        {
            if (isboostavailablefir)
            {
                if (isboostavailablesec)
                    return true; // 0 0 0 0
                if (sec[0] > 63)
                    return sec[0] == other.sec[0]; // 0 0 x 0
                return sec[4] == other.sec[4];     // 0 0 0 x
            }
            if (isboostavailablesec)
            {
                if (fir[0] > 63)
                    return fir[0] == other.fir[0]; // x 0 0 0
                return fir[4] == other.fir[4];     // 0 x 0 0
            }
            if (fir[0] > 63)
            {
                if (sec[0] > 63)
                    return fir[0] == other.fir[0] and sec[0] == other.sec[0]; // x 0 x 0
                return fir[0] == other.fir[0] and sec[4] == other.sec[4];     // x 0 0 x
            }
            if (sec[0] > 63)
                return fir[4] == other.fir[4] and sec[0] == other.sec[0]; // 0 x x 0
            return fir[4] == other.fir[4] and sec[4] == other.sec[4];     // 0 x 0 x
        }
        return false;
    }
};

void printfield(field &todisplay)
{
    cout << "Virus: " << todisplay.firvirus << "         Link: " << todisplay.firlink << endl;
    for (int i = 63; i > -1; --i)
    {
        if ((todisplay.firl >> i) & 1)
        {
            for (int u = 0; u < todisplay.firlinkindex; ++u)
                if (todisplay.fir[u] > 0 and (todisplay.fir[u] & 63) == i and (todisplay.fir[u] & 64) == 64)
                {
                    cout << "\033[32m[\033[0m\033[34mL\033[0m\033[32m]\033[0m";
                    goto end;
                }
            cout << "[\033[32mL\033[0m]";
        }
        else if ((todisplay.firv >> i) & 1)
        {
            for (int u = 4; u < todisplay.firvirusindex; ++u)
                if (todisplay.fir[u] > 0 and (todisplay.fir[u] & 63) == i and (todisplay.fir[u] & 64) == 64)
                {
                    cout << "\033[32m[\033[0m\033[34mV\033[0m\033[32m]\033[0m";
                    goto end;
                }
            cout << "[\033[32mV\033[0m]";
        }
        else if ((todisplay.secl >> i) & 1)
        {
            for (int u = 0; u < todisplay.seclinkindex; ++u)
                if (todisplay.sec[u] > 0 and (todisplay.sec[u] & 63) == i and (todisplay.sec[u] & 64) == 64)
                {
                    cout << "\033[31m[\033[0m\033[34mL\033[0m\033[31m]\033[0m";
                    goto end;
                }
            cout << "[\033[31mL\033[0m]";
        }
        else if ((todisplay.secv >> i) & 1)
        {
            for (int u = 4; u < todisplay.secvirusindex; ++u)
                if (todisplay.sec[u] > 0 and (todisplay.sec[u] & 63) == i and (todisplay.sec[u] & 64) == 64)
                {
                    cout << "\033[31m[\033[0m\033[34mV\033[0m\033[31m]\033[0m";
                    goto end;
                }
            cout << "[\033[31mV\033[0m]";
        }
        else
            cout << "[ ]";
    end:
        if (i % 8 == 0)
            cout << endl;
    }
    cout << "Virus: " << todisplay.secvirus << "         Link: " << todisplay.seclink << endl;
}

void generatexfield(field &togenerate, const bool player, const int x)
{
    if (player)
    {
        togenerate.firv = 0;
        togenerate.firl = 0;
        int linkindex = 0, virusindex = 4;
        if (x & 1)
        {
            togenerate.fir[virusindex] = 63;
            togenerate.firv |= (1ULL << 63);
            virusindex++;
        }
        else
        {
            togenerate.fir[linkindex] = 63;
            togenerate.firl |= (1ULL << 63);
            linkindex++;
        }
        if (x & 2)
        {
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
        if (x & 4)
        {
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
        if (x & 8)
        {
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
        if (x & 16)
        {
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
        if (x & 32)
        {
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
        if (x & 64)
        {
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
        if (x & 128)
        {
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
    }
    else
    {
        togenerate.secv = 0;
        togenerate.secl = 0;
        int linkindex = 0, virusindex = 4;
        if (x & 1)
        {
            togenerate.sec[virusindex] = 0;
            togenerate.secv |= (1ULL << 0);
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 0;
            togenerate.secl |= (1ULL << 0);
            linkindex++;
        }
        if (x & 2)
        {
            togenerate.sec[virusindex] = 1;
            togenerate.secv |= (1ULL << 1);
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 1;
            togenerate.secl |= (1ULL << 1);
            linkindex++;
        }
        if (x & 4)
        {
            togenerate.sec[virusindex] = 2;
            togenerate.secv |= (1ULL << 2);
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 2;
            togenerate.secl |= (1ULL << 2);
            linkindex++;
        }
        if (x & 8)
        {
            togenerate.sec[virusindex] = 11;
            togenerate.secv |= (1ULL << 11);
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 11;
            togenerate.secl |= (1ULL << 11);
            linkindex++;
        }
        if (x & 16)
        {
            togenerate.sec[virusindex] = 12;
            togenerate.secv |= (1ULL << 12);
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 12;
            togenerate.secl |= (1ULL << 12);
            linkindex++;
        }
        if (x & 32)
        {
            togenerate.sec[virusindex] = 5;
            togenerate.secv |= (1ULL << 5);
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 5;
            togenerate.secl |= (1ULL << 5);
            linkindex++;
        }
        if (x & 64)
        {
            togenerate.sec[virusindex] = 6;
            togenerate.secv |= (1ULL << 6);
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 6;
            togenerate.secl |= (1ULL << 6);
            linkindex++;
        }
        if (x & 128)
        {
            togenerate.sec[virusindex] = 7;
            togenerate.secv |= (1ULL << 7);
            virusindex++;
        }
        else
        {
            togenerate.sec[linkindex] = 7;
            togenerate.secl |= (1ULL << 7);
            linkindex++;
        }
    }
}

inline void removesecondlink(field &position, const int &coords)
{
    switch (position.seclinkindex)
    {
    case 1:
        if (position.sec[0] & 64)
            position.isboostavailablesec = true;
        --position.seclinkindex;
        position.sec[0] = 0;
        return;
    case 2:
        if ((position.sec[0] & 63) == coords)
        {
            if (position.sec[0] & 64)
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
        if ((position.sec[0] & 63) == coords)
        {
            if (position.sec[0] & 64)
                position.isboostavailablesec = true;
            --position.seclinkindex;
            position.sec[0] = position.sec[2];
            position.sec[2] = 0;
            return;
        }
        if (position.sec[1] == coords)
        {
            --position.seclinkindex;
            position.sec[1] = position.sec[2];
            position.sec[2] = 0;
            return;
        }
        --position.seclinkindex;
        position.sec[2] = 0;
        return;
    case 4:
        if ((position.sec[0] & 63) == coords)
        {
            if (position.sec[0] & 64)
                position.isboostavailablesec = true;
            --position.seclinkindex;
            position.sec[0] = position.sec[3];
            position.sec[3] = 0;
            return;
        }
        if (position.sec[1] == coords)
        {
            --position.seclinkindex;
            position.sec[1] = position.sec[3];
            position.sec[3] = 0;
            return;
        }
        if (position.sec[2] == coords)
        {
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

inline void removefirstlink(field &position, const int &coords)
{
    switch (position.firlinkindex)
    {
    case 1:
        if (position.fir[0] & 64)
            position.isboostavailablefir = true;
        --position.firlinkindex;
        position.fir[0] = 56;
        return;
    case 2:
        if ((position.fir[0] & 63) == coords)
        {
            if (position.fir[0] & 64)
                position.isboostavailablefir = true;
            --position.firlinkindex;
            position.fir[0] = position.fir[1];
            position.fir[1] = 56;
            return;
        }
        --position.firlinkindex;
        position.fir[1] = 56;
        return;
    case 3:
        if ((position.fir[0] & 63) == coords)
        {
            if (position.fir[0] & 64)
                position.isboostavailablefir = true;
            --position.firlinkindex;
            position.fir[0] = position.fir[2];
            position.fir[2] = 56;
            return;
        }
        if (position.fir[1] == coords)
        {
            --position.firlinkindex;
            position.fir[1] = position.fir[2];
            position.fir[2] = 56;
            return;
        }
        --position.firlinkindex;
        position.fir[2] = 56;
        return;
    case 4:
        if ((position.fir[0] & 63) == coords)
        {
            if (position.fir[0] & 64)
                position.isboostavailablefir = true;
            --position.firlinkindex;
            position.fir[0] = position.fir[3];
            position.fir[3] = 56;
            return;
        }
        if (position.fir[1] == coords)
        {
            --position.firlinkindex;
            position.fir[1] = position.fir[3];
            position.fir[3] = 56;
            return;
        }
        if (position.fir[2] == coords)
        {
            --position.firlinkindex;
            position.fir[2] = position.fir[3];
            position.fir[3] = 56;
            return;
        }
        --position.firlinkindex;
        position.fir[3] = 56;
        return;
    }
}

inline void removesecondvirus(field &position, const int &coords)
{
    switch (position.secvirusindex)
    {
    case 5:
        if (position.sec[4] & 64)
            position.isboostavailablesec = true;
        --position.secvirusindex;
        position.sec[4] = 0;
        return;
    case 6:
        if ((position.sec[4] & 63) == coords)
        {
            if (position.sec[4] & 64)
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
        if ((position.sec[4] & 63) == coords)
        {
            if (position.sec[4] & 64)
                position.isboostavailablesec = true;
            --position.secvirusindex;
            position.sec[4] = position.sec[6];
            position.sec[6] = 0;
            return;
        }
        if (position.sec[5] == coords)
        {
            --position.secvirusindex;
            position.sec[5] = position.sec[6];
            position.sec[6] = 0;
            return;
        }
        --position.secvirusindex;
        position.sec[6] = 0;
        return;
    case 8:
        if ((position.sec[4] & 63) == coords)
        {
            if (position.sec[4] & 64)
                position.isboostavailablesec = true;
            --position.secvirusindex;
            position.sec[4] = position.sec[7];
            position.sec[7] = 0;
            return;
        }
        if (position.sec[5] == coords)
        {
            --position.secvirusindex;
            position.sec[5] = position.sec[7];
            position.sec[7] = 0;
            return;
        }
        if (position.sec[6] == coords)
        {
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

inline void removefirstvirus(field &position, const int &coords)
{
    switch (position.firvirusindex)
    {
    case 5:
        if (position.fir[4] & 64)
            position.isboostavailablefir = true;
        --position.firvirusindex;
        position.fir[4] = 56;
        return;
    case 6:
        if ((position.fir[4] & 63) == coords)
        {
            if (position.fir[4] & 64)
                position.isboostavailablefir = true;
            --position.firvirusindex;
            position.fir[4] = position.fir[5];
            position.fir[5] = 56;
            return;
        }
        --position.firvirusindex;
        position.fir[5] = 56;
        return;
    case 7:
        if ((position.fir[4] & 63) == coords)
        {
            if (position.fir[4] & 64)
                position.isboostavailablefir = true;
            --position.firvirusindex;
            position.fir[4] = position.fir[6];
            position.fir[6] = 56;
            return;
        }
        if (position.fir[5] == coords)
        {
            --position.firvirusindex;
            position.fir[5] = position.fir[6];
            position.fir[6] = 56;
            return;
        }
        --position.firvirusindex;
        position.fir[6] = 56;
        return;
    case 8:
        if ((position.fir[4] & 63) == coords)
        {
            if (position.fir[4] & 64)
                position.isboostavailablefir = true;
            --position.firvirusindex;
            position.fir[4] = position.fir[7];
            position.fir[7] = 56;
            return;
        }
        if (position.fir[5] == coords)
        {
            --position.firvirusindex;
            position.fir[5] = position.fir[7];
            position.fir[7] = 56;
            return;
        }
        if (position.fir[6] == coords)
        {
            --position.firvirusindex;
            position.fir[6] = position.fir[7];
            position.fir[7] = 56;
            return;
        }
        --position.firvirusindex;
        position.fir[7] = 56;
        return;
    }
}

vector<field> possiblemoves(field &position, const bool player)
{
    vector<field> nplusone;
    nplusone.reserve(40);
    if (player)
    {
        if (__builtin_expect(position.firl & 24, 0))
        {
            __builtin_assume(position.firlinkindex > 0 and position.firlinkindex < 5);
#pragma clang loop unroll_count(4)
            for (auto i = 0; i < position.firlinkindex; ++i)
            {
                const int t2 = (position.fir[i] & 63);
                if (t2 == 3 or t2 == 4)
                {
                    field temp = position;
                    if (position.fir[i] & 64)
                        temp.isboostavailablefir = true;
                    --temp.firlinkindex;
                    temp.fir[i] = temp.fir[temp.firlinkindex];
                    temp.fir[temp.firlinkindex] = 56;
                    temp.firl ^= (1ULL << t2);
                    ++temp.firlink;
                    nplusone.push_back(temp);
                }
            }
        }
        if (position.isboostavailablefir)
        {
            __builtin_assume(position.firvirusindex >= 4 and position.firvirusindex <= 8);
#pragma clang loop unroll_count(4)
            for (int i = 4; i < position.firvirusindex; ++i)
            {
                position.fir[i] ^= 64;
                position.isboostavailablefir = false;
                swap(position.fir[i], position.fir[4]);
                nplusone.push_back(position);
                swap(position.fir[i], position.fir[4]);
                position.fir[i] ^= 64;
                position.isboostavailablefir = true;
            }
            __builtin_assume(position.firlinkindex >= 0 and position.firlinkindex <= 4);
#pragma clang loop unroll_count(4)
            for (int i = 0; i < position.firlinkindex; ++i)
            {
                position.fir[i] ^= 64;
                position.isboostavailablefir = false;
                swap(position.fir[i], position.fir[0]);
                nplusone.push_back(position);
                swap(position.fir[i], position.fir[0]);
                position.fir[i] ^= 64;
                position.isboostavailablefir = true;
            }
        }
        const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
        if (firewallsec < 0)
        {
            if (position.isboostavailablefir == false)
            {
                int i;
                if (position.fir[0] & 64)
                    i = 0;
                else
                    i = 4;
                const int coords = (position.fir[i] & 63), x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                if (mshift >> 8)
                {
                    uint64_t shiftconst = (mshift >> 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firlink;
                                temp.secl ^= shiftconst;
                                removesecondlink(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firvirus;
                                temp.secv ^= shiftconst;
                                removesecondvirus(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            if (mshift >> 16)
                            {
                                uint64_t shiftconstsec = (mshift >> 16);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 16;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 16);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 16;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 16);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 16;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] += 16;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            position.fir[i] -= 8;
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            nplusone.push_back(position);
                            position.fir[i] += 8;
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            if (x < 7)
                            {
                                uint64_t shiftconstsec = (mshift >> 7);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 7);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 7);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] += 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if (x > 0)
                            {
                                uint64_t shiftconstsec = (mshift >> 9);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 9);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 9);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] += 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                        }
                    }
                }
                if (x < 7)
                {
                    uint64_t shiftconst = (mshift << 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firlink;
                                temp.secl ^= shiftconst;
                                removesecondlink(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firvirus;
                                temp.secv ^= shiftconst;
                                removesecondvirus(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            if (x < 6)
                            {
                                uint64_t shiftconstsec = (mshift << 2);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 2;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 2);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 2;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 2);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 2;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] -= 2;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            ++position.fir[i];
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            nplusone.push_back(position);
                            --position.fir[i];
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            if ((firmask & (mshift >> 8)) or (secmask & (mshift >> 8)))
                            {
                                uint64_t shiftconstsec = (mshift >> 7);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 7);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 7);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] += 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if ((firmask & (mshift << 8)) or (secmask & (mshift << 8)))
                            {
                                uint64_t shiftconstsec = (mshift << 9);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 9);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 9);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] -= 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                        }
                    }
                }
                if (x > 0)
                {
                    uint64_t shiftconst = (mshift >> 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                --temp.fir[i];
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firlink;
                                temp.secl ^= shiftconst;
                                removesecondlink(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                --temp.fir[i];
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firvirus;
                                temp.secv ^= shiftconst;
                                removesecondvirus(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            if (x > 1)
                            {
                                uint64_t shiftconstsec = (mshift >> 2);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 2;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 2);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 2;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 2);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 2;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] += 2;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            --position.fir[i];
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            nplusone.push_back(position);
                            ++position.fir[i];
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            if ((firmask & (mshift >> 8)) or (secmask & (mshift >> 8)))
                            {
                                uint64_t shiftconstsec = (mshift >> 9);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 9);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 9);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] += 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if ((firmask & (mshift << 8)) or (secmask & (mshift << 8)))
                            {
                                uint64_t shiftconstsec = (mshift << 7);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 7);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 7);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] -= 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                        }
                    }
                }
                if (mshift << 8)
                {
                    uint64_t shiftconst = (mshift << 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firlink;
                                temp.secl ^= shiftconst;
                                removesecondlink(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firvirus;
                                temp.secv ^= shiftconst;
                                removesecondvirus(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.fir[i] += 8;
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            nplusone.push_back(position);
                            position.fir[i] -= 8;
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            if (x < 7)
                            {
                                uint64_t shiftconstsec = (mshift << 9);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 9);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 9);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] -= 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if (x > 0)
                            {
                                uint64_t shiftconstsec = (mshift << 7);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 7);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 7);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] -= 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if (mshift << 16)
                            {
                                uint64_t shiftconstsec = (mshift << 16);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 16;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 16);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 16;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 16);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 16;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.fir[i] -= 16;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            __builtin_assume(position.firvirusindex >= 4 and position.firvirusindex <= 8);
            for (int i = 4 + ((position.fir[4] >> 6) & 1); i < position.firvirusindex; ++i)
            {
                const int coords = position.fir[i], x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                uint64_t shiftconst;
                if (mshift >> 8)
                {
                    shiftconst = (mshift >> 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.fir[i] -= 8;
                            position.firv ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            position.fir[i] += 8;
                            position.firv ^= (shiftconst | mshift);
                        }
                    }
                }
                if (x < 7)
                {
                    shiftconst = (mshift << 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            ++position.fir[i];
                            position.firv ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            --position.fir[i];
                            position.firv ^= (shiftconst | mshift);
                        }
                    }
                }
                if (x > 0)
                {
                    shiftconst = (mshift >> 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                --temp.fir[i];
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                --temp.fir[i];
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            --position.fir[i];
                            position.firv ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            ++position.fir[i];
                            position.firv ^= (shiftconst | mshift);
                        }
                    }
                }
                if (mshift << 8)
                {
                    shiftconst = (mshift << 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.fir[i] += 8;
                            position.firv ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            position.fir[i] -= 8;
                            position.firv ^= (shiftconst | mshift);
                        }
                    }
                }
            }
            __builtin_assume(position.firlinkindex >= 0 and position.firlinkindex <= 4);
            for (int i = ((position.fir[0] >> 6) & 1); i < position.firlinkindex; ++i)
            {
                const int coords = position.fir[i], x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                uint64_t shiftconst;
                if (mshift >> 8)
                {
                    shiftconst = (mshift >> 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.fir[i] -= 8;
                            position.firl ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            position.fir[i] += 8;
                            position.firl ^= (shiftconst | mshift);
                        }
                    }
                }
                if (x < 7)
                {
                    shiftconst = (mshift << 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            ++position.fir[i];
                            position.firl ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            --position.fir[i];
                            position.firl ^= (shiftconst | mshift);
                        }
                    }
                }
                if (x > 0)
                {
                    shiftconst = (mshift >> 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                --temp.fir[i];
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                --temp.fir[i];
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            --position.fir[i];
                            position.firl ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            ++position.fir[i];
                            position.firl ^= (shiftconst | mshift);
                        }
                    }
                } 
                if (mshift << 8)
                {
                    shiftconst = (mshift << 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.fir[i] += 8;
                            position.firl ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            position.fir[i] -= 8;
                            position.firl ^= (shiftconst | mshift);
                        }
                    }
                }
            }
        }
        else
        {
            // todo
        }
    }
    else
    {
        if (__builtin_expect(position.secl & 1729382256910270464ULL, 0))
        {
            __builtin_assume(position.seclinkindex > 0 and position.seclinkindex < 5);
#pragma clang loop unroll_count(4)
            for (auto i = 0; i < position.seclinkindex; ++i)
            {
                const int t2 = (position.sec[i] & 63);
                if (t2 == 59 or t2 == 60)
                {
                    field temp = position;
                    if (position.sec[i] & 64)
                        temp.isboostavailablesec = true;
                    --temp.seclinkindex;
                    temp.sec[i] = temp.sec[temp.seclinkindex];
                    temp.sec[temp.seclinkindex] = 0;
                    temp.secl ^= (1ULL << t2);
                    ++temp.seclink;
                    nplusone.push_back(temp);
                }
            }
        }
        if (position.isboostavailablesec)
        {
            __builtin_assume(position.secvirusindex >= 4 and position.secvirusindex <= 8);
#pragma clang loop unroll_count(4)
            for (auto i = 4; i < position.secvirusindex; ++i)
            {
                position.sec[i] ^= 64;
                position.isboostavailablesec = false;
                swap(position.sec[i], position.sec[4]);
                nplusone.push_back(position);
                swap(position.sec[i], position.sec[4]);
                position.sec[i] ^= 64;
                position.isboostavailablesec = true;
            }
            __builtin_assume(position.seclinkindex >= 0 and position.seclinkindex <= 4);
#pragma clang loop unroll_count(4)
            for (auto i = 0; i < position.seclinkindex; ++i)
            {
                position.sec[i] ^= 64;
                position.isboostavailablesec = false;
                swap(position.sec[i], position.sec[0]);
                nplusone.push_back(position);
                swap(position.sec[i], position.sec[0]);
                position.sec[i] ^= 64;
                position.isboostavailablesec = true;
            }
        }
        const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
        if (firewallfir < 0)
        {
            if (position.isboostavailablesec == false)
            {
                int i;
                if (position.sec[0] & 64)
                    i = 0;
                else
                    i = 4;
                const int coords = (position.sec[i] & 63), x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                if (mshift << 8)
                {
                    uint64_t shiftconst = (mshift << 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.seclink;
                                temp.firl ^= shiftconst;
                                removefirstlink(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.secvirus;
                                temp.firv ^= shiftconst;
                                removefirstvirus(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            if (mshift << 16)
                            {
                                uint64_t shiftconstsec = (mshift << 16);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 16;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 16);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 16;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 16);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 16;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] -= 16;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            position.sec[i] += 8;
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            nplusone.push_back(position);
                            position.sec[i] -= 8;
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            if (x > 0)
                            {
                                uint64_t shiftconstsec = (mshift << 7);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 7);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 7);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] -= 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if (x < 7)
                            {
                                uint64_t shiftconstsec = (mshift << 9);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 9);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 9);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] -= 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                        }
                    }
                }
                if (x > 0)
                {
                    uint64_t shiftconst = (mshift >> 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                --temp.sec[i];
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.seclink;
                                temp.firl ^= shiftconst;
                                removefirstlink(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                --temp.sec[i];
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.secvirus;
                                temp.firv ^= shiftconst;
                                removefirstvirus(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            if (x > 1)
                            {
                                uint64_t shiftconstsec = (mshift >> 2);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 2;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 2);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 2;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 2);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 2;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] += 2;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            --position.sec[i];
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            nplusone.push_back(position);
                            ++position.sec[i];
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            if ((secmask & (mshift << 8)) or (firmask & (mshift << 8)))
                            {
                                uint64_t shiftconstsec = (mshift << 7);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 7);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 7);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] -= 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if ((secmask & (mshift >> 8)) or (firmask & (mshift >> 8)))
                            {
                                uint64_t shiftconstsec = (mshift >> 9);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 9);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 9);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] += 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                        }
                    }
                }
                if (x < 7)
                {
                    uint64_t shiftconst = (mshift << 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.seclink;
                                temp.firl ^= shiftconst;
                                removefirstlink(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.secvirus;
                                temp.firv ^= shiftconst;
                                removefirstvirus(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            if (x < 6)
                            {
                                uint64_t shiftconstsec = (mshift << 2);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 2;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 2);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 2;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 2);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 2;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] -= 2;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            ++position.sec[i];
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            nplusone.push_back(position);
                            --position.sec[i];
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            if ((secmask & (mshift << 8)) or (firmask & (mshift << 8)))
                            {
                                uint64_t shiftconstsec = (mshift << 9);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 9);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 9);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] -= 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if ((secmask & (mshift >> 8)) or (firmask & (mshift >> 8)))
                            {
                                uint64_t shiftconstsec = (mshift >> 7);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 7);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 7);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] += 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                        }
                    }
                }
                if (mshift >> 8)
                {
                    uint64_t shiftconst = (mshift >> 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.seclink;
                                temp.firl ^= shiftconst;
                                removefirstlink(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.secvirus;
                                temp.firv ^= shiftconst;
                                removefirstvirus(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.sec[i] -= 8;
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            nplusone.push_back(position);
                            position.sec[i] += 8;
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            if (x > 0)
                            {
                                uint64_t shiftconstsec = (mshift >> 9);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 9);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 9);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] += 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if (x < 7)
                            {
                                uint64_t shiftconstsec = (mshift >> 7);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 7);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 7);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] += 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                            if (mshift >> 16)
                            {
                                uint64_t shiftconstsec = (mshift >> 16);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 16;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 16);
                                            nplusone.push_back(temp);
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 16;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 16);
                                            nplusone.push_back(temp);
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 16;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        nplusone.push_back(position);
                                        position.sec[i] += 16;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            __builtin_assume(position.secvirusindex >= 4 and position.secvirusindex <= 8);
            for (auto i = 4 + ((position.sec[4] >> 6) & 1); i < position.secvirusindex; ++i)
            {
                const int coords = position.sec[i], x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                uint64_t shiftconst;
                if (mshift << 8)
                {
                    shiftconst = (mshift << 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.sec[i] += 8;
                            position.secv ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            position.sec[i] -= 8;
                            position.secv ^= (shiftconst | mshift);
                        }
                    }
                }
                if (x > 0)
                {
                    shiftconst = (mshift >> 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                --temp.sec[i];
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                --temp.sec[i];
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            --position.sec[i];
                            position.secv ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            ++position.sec[i];
                            position.secv ^= (shiftconst | mshift);
                        }
                    }
                }
                if (x < 7)
                {
                    shiftconst = (mshift << 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            ++position.sec[i];
                            position.secv ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            --position.sec[i];
                            position.secv ^= (shiftconst | mshift);
                        }
                    }
                }
                if (mshift >> 8)
                {
                    shiftconst = (mshift >> 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.sec[i] -= 8;
                            position.secv ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            position.sec[i] += 8;
                            position.secv ^= (shiftconst | mshift);
                        }
                    }
                }
            }
            __builtin_assume(position.seclinkindex >= 0 and position.seclinkindex <= 4);
            for (auto i = ((position.sec[0] >> 6) & 1); i < position.seclinkindex; ++i)
            {
                const int coords = position.sec[i], x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                uint64_t shiftconst;
                if (mshift << 8)
                {
                    shiftconst = (mshift << 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords + 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.sec[i] += 8;
                            position.secl ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            position.sec[i] -= 8;
                            position.secl ^= (shiftconst | mshift);
                        }
                    }
                }
                if (x > 0)
                {
                    shiftconst = (mshift >> 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                --temp.sec[i];
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                --temp.sec[i];
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords - 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            --position.sec[i];
                            position.secl ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            ++position.sec[i];
                            position.secl ^= (shiftconst | mshift);
                        }
                    }
                }
                if (x < 7)
                {
                    shiftconst = (mshift << 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords + 1);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            ++position.sec[i];
                            position.secl ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            --position.sec[i];
                            position.secl ^= (shiftconst | mshift);
                        }
                    }
                }
                if (mshift >> 8)
                {
                    shiftconst = (mshift >> 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords - 8);
                                nplusone.push_back(temp);
                            }
                        }
                        else
                        {
                            position.sec[i] -= 8;
                            position.secl ^= (shiftconst | mshift);
                            nplusone.push_back(position);
                            position.sec[i] += 8;
                            position.secl ^= (shiftconst | mshift);
                        }
                    }
                }
            }
        }
        else
        {
            // todo
        }
    }
    return nplusone;
}

const int mincachedepth = 2, mincachedepthfull = 2, maxthreads = 50, mindepthformultithreadedsearch = 9;

int minimax(int depth, int alpha, int beta, const bool player, field &position, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache, bool &terminate);

inline void minimaxfullfir(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache, bool &terminate)
{
    if (depth == 0)
        reschild = position.evaluatefir();
    else
        reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
}

inline void minimaxscoutfir(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache, bool &terminate)
{
    if (depth == 0)
        reschild = position.evaluatefir();
    else
    {
        if (beta < MAX)
        {
            reschild = minimax(depth, beta - 1, beta, true, position, cache, terminate);
            if (reschild > alpha and reschild < beta)
                reschild = minimax(depth, alpha, reschild, true, position, cache, terminate);
        }
        else
            reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
    }
}

inline void minimaxfullsec(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache, bool &terminate)
{
    if (depth == 0)
        reschild = position.evaluatesec();
    else
        reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
}

inline void minimaxscoutsec(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache, bool &terminate)
{
    if (depth == 0)
        reschild = position.evaluatesec();
    else
    {
        if (alpha > MIN and depth > 0)
        {
            reschild = minimax(depth, alpha, alpha + 1, false, position, cache, terminate);
            if (reschild > alpha and reschild < beta)
                reschild = minimax(depth, reschild, beta, false, position, cache, terminate);
        }
        else
            reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
    }
}

int minimax(int depth, int alpha, int beta, const bool player, field &position, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache, bool &terminate)
{
    if (player)
    {
        if (terminate)
            return beta;
        if (depth == 0)
            return position.evaluatefir();
        --depth;
        if (position.firlink == 3)
        {
            if (position.firl & 24)
                return (16384 * depth);
            if (position.secl)
            {
                const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
                if (((position.secl >> 8) & firmask) or ((position.secl << 8) & firmask))
                    return (16384 * depth);
                __builtin_assume(position.seclinkindex > 0 and position.seclinkindex < 5);
#pragma clang loop unroll_count(4)
                for (int i = 0; i < position.seclinkindex; ++i)
                {
                    const int cachedpos = position.sec[i], t = (cachedpos & 7);
                    if (t < 7)
                        if (firmask & (1ULL << (cachedpos + 1)))
                            return (16384 * depth);
                    if (t > 0)
                        if (firmask & (1ULL << (cachedpos - 1)))
                            return (16384 * depth);
                }
                if (position.fir[4] > 64)
                {
                    const int cachedpos = (position.fir[4] & 63), t = (cachedpos & 7);
                    if (cachedpos < 56)
                    {
                        if (cachedpos < 48)
                            if (position.secl & (1ULL << (cachedpos + 16)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (16384 * depth);
                        if (t < 7)
                            if (position.secl & (1ULL << (cachedpos + 9)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos + 1))) == 0 or ((position.secv | firmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (16384 * depth);
                        if (t > 0)
                            if (position.secl & (1ULL << (cachedpos + 7)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos - 1))) == 0 or ((position.secv | firmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (16384 * depth);
                    }
                    if (cachedpos > 7)
                    {
                        if (cachedpos > 15)
                            if (position.secl & (1ULL << (cachedpos - 16)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (16384 * depth);
                        if (t < 7)
                            if (position.secl & (1ULL << (cachedpos - 7)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos + 1))) == 0 or ((position.secv | firmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (16384 * depth);
                        if (t > 0)
                            if (position.secl & (1ULL << (cachedpos - 9)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos - 1))) == 0 or ((position.secv | firmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (16384 * depth);
                    }
                    if (t < 6)
                        if (position.secl & (1ULL << (cachedpos + 2)))
                            if (((position.secv | firmask) & (1ULL << (cachedpos + 1))) == 0)
                                return (16384 * depth);
                    if (t > 1)
                        if (position.secl & (1ULL << (cachedpos - 2)))
                            if (((position.secv | firmask) & (1ULL << (cachedpos - 1))) == 0)
                                return (16384 * depth);
                }
                if (position.fir[0] > 63)
                {
                    const int cachedpos = (position.fir[0] & 63), t = (cachedpos & 7);
                    // if(cachedpos == 5 or cachedpos == 4 or cachedpos == 3 or cachedpos == 2 or (cachedpos == 11 and ((secmask | firmask) & (1ULL << 3)) == 0) or (cachedpos == 12 and ((secmask | firmask) & (1ULL << 4)) == 0))
                    //     return (16384 * depth);
                    if (cachedpos < 56)
                    {
                        if (cachedpos < 48)
                            if (position.secl & (1ULL << (cachedpos + 16)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (16384 * depth);
                        if (t < 7)
                            if (position.secl & (1ULL << (cachedpos + 9)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos + 1))) == 0 or ((position.secv | firmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (16384 * depth);
                        if (t > 0)
                            if (position.secl & (1ULL << (cachedpos + 7)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos - 1))) == 0 or ((position.secv | firmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (16384 * depth);
                    }
                    if (cachedpos > 7)
                    {
                        if (cachedpos > 15)
                            if (position.secl & (1ULL << (cachedpos - 16)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (16384 * depth);
                        if (t < 7)
                            if (position.secl & (1ULL << (cachedpos - 7)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos + 1))) == 0 or ((position.secv | firmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (16384 * depth);
                        if (t > 0)
                            if (position.secl & (1ULL << (cachedpos - 9)))
                                if (((position.secv | firmask) & (1ULL << (cachedpos - 1))) == 0 or ((position.secv | firmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (16384 * depth);
                    }
                    if (t < 6)
                        if (position.secl & (1ULL << (cachedpos + 2)))
                            if (((position.secv | firmask) & (1ULL << (cachedpos + 1))) == 0)
                                return (16384 * depth);
                    if (t > 1)
                        if (position.secl & (1ULL << (cachedpos - 2)))
                            if (((position.secv | firmask) & (1ULL << (cachedpos - 1))) == 0)
                                return (16384 * depth);
                }
            }
        }
        int alphabeg;
        if (depth > mincachedepth)
        {
            auto it = cache[depth].find(position);
            if (it != cache[depth].end())
            {
                ttentry entry = it->second;
                if (__builtin_expect(entry.flag & 1, 0))
                {
                    if (entry.score <= alpha) // if current alpha >= cached alpha then the alpha during evaluation wont change, thus we can return the current alpha
                        return alpha;
                    if (entry.flag > 1) // if the cached alpha is exact and it is bigger than the current alpha (because of the condition above) then we can return it
                        return entry.score;
                    beta = min(beta, entry.score);
                    // cached alpha is lower bound
                }
                else
                {
                    if (entry.score > alpha)
                    {
                        if (entry.score >= beta)
                            return entry.score;
                        alpha = entry.score;
                    }
                }
            }
            alphabeg = alpha;
        }
        if (__builtin_expect(position.firl & 24, 0))
        {
            __builtin_assume(position.firlinkindex > 0 and position.firlinkindex < 5);
#pragma clang loop unroll_count(4)
            for (auto i = 0; i < position.firlinkindex; ++i)
            {
                const int t2 = (position.fir[i] & 63);
                if (t2 == 3 or t2 == 4)
                {
                    field temp = position;
                    if (position.fir[i] & 64)
                        temp.isboostavailablefir = true;
                    --temp.firlinkindex;
                    temp.fir[i] = temp.fir[temp.firlinkindex];
                    temp.fir[temp.firlinkindex] = 56;
                    temp.firl ^= (1ULL << t2);
                    ++temp.firlink;
                    int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                    if (reschild > alpha)
                    {
                        if (beta <= reschild)
                        {
                            if (depth > mincachedepth)
                                cache[depth][position] = {reschild, 0};
                            return reschild;
                        }
                        alpha = reschild;
                    }
                }
            }
        }
        if (position.isboostavailablefir)
        {
            __builtin_assume(position.firvirusindex >= 4 and position.firvirusindex <= 8);
#pragma clang loop unroll_count(4)
            for (int i = 4; i < position.firvirusindex; ++i)
            {
                position.fir[i] ^= 64;
                position.isboostavailablefir = false;
                swap(position.fir[i], position.fir[4]);
                int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                swap(position.fir[i], position.fir[4]);
                position.fir[i] ^= 64;
                position.isboostavailablefir = true;
                if (reschild > alpha)
                {
                    if (beta <= reschild)
                    {
                        if (depth > mincachedepth)
                            cache[depth][position] = {reschild, 0};
                        return reschild;
                    }
                    alpha = reschild;
                }
            }
            __builtin_assume(position.firlinkindex >= 0 and position.firlinkindex <= 4);
#pragma clang loop unroll_count(4)
            for (int i = 0; i < position.firlinkindex; ++i)
            {
                position.fir[i] ^= 64;
                position.isboostavailablefir = false;
                swap(position.fir[i], position.fir[0]);
                int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                swap(position.fir[i], position.fir[0]);
                position.fir[i] ^= 64;
                position.isboostavailablefir = true;
                if (reschild > alpha)
                {
                    if (beta <= reschild)
                    {
                        if (depth > mincachedepth)
                            cache[depth][position] = {reschild, 0};
                        return reschild;
                    }
                    alpha = reschild;
                }
            }
        }
        const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
        if (firewallsec < 0)
        {
            if (position.isboostavailablefir == false)
            {
                int i;
                if (position.fir[0] & 64)
                    i = 0;
                else
                    i = 4;
                const int coords = (position.fir[i] & 63), x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                if (mshift >> 8)
                {
                    uint64_t shiftconst = (mshift >> 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firlink;
                                temp.secl ^= shiftconst;
                                removesecondlink(temp, coords - 8);
                                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firvirus;
                                temp.secv ^= shiftconst;
                                removesecondvirus(temp, coords - 8);
                                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                        }
                        else
                        {
                            if (mshift >> 16)
                            {
                                uint64_t shiftconstsec = (mshift >> 16);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 16;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 16);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 16;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 16);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 16;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] += 16;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            alpha = reschild;
                                        }
                                    }
                                }
                            }
                            position.fir[i] -= 8;
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                            position.fir[i] += 8;
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                            if (x < 7)
                            {
                                uint64_t shiftconstsec = (mshift >> 7);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 7);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 7);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] += 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            alpha = reschild;
                                        }
                                    }
                                }
                            }
                            if (x > 0)
                            {
                                uint64_t shiftconstsec = (mshift >> 9);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 9);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 9);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] += 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
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
                }
                if (x < 7)
                {
                    uint64_t shiftconst = (mshift << 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firlink;
                                temp.secl ^= shiftconst;
                                removesecondlink(temp, coords + 1);
                                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firvirus;
                                temp.secv ^= shiftconst;
                                removesecondvirus(temp, coords + 1);
                                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                        }
                        else
                        {
                            if (x < 6)
                            {
                                uint64_t shiftconstsec = (mshift << 2);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 2;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 2);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 2;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 2);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 2;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] -= 2;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            alpha = reschild;
                                        }
                                    }
                                }
                            }
                            ++position.fir[i];
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                            --position.fir[i];
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                            if ((firmask & (mshift >> 8)) or (secmask & (mshift >> 8)))
                            {
                                uint64_t shiftconstsec = (mshift >> 7);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 7);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 7);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] += 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            alpha = reschild;
                                        }
                                    }
                                }
                            }
                            if ((firmask & (mshift << 8)) or (secmask & (mshift << 8)))
                            {
                                uint64_t shiftconstsec = (mshift << 9);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 9);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 9);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] -= 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
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
                }
                if (x > 0)
                {
                    uint64_t shiftconst = (mshift >> 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                --temp.fir[i];
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firlink;
                                temp.secl ^= shiftconst;
                                removesecondlink(temp, coords - 1);
                                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                --temp.fir[i];
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firvirus;
                                temp.secv ^= shiftconst;
                                removesecondvirus(temp, coords - 1);
                                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                        }
                        else
                        {
                            if (x > 1)
                            {
                                uint64_t shiftconstsec = (mshift >> 2);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 2;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 2);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 2;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 2);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 2;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] += 2;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            alpha = reschild;
                                        }
                                    }
                                }
                            }
                            --position.fir[i];
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                            ++position.fir[i];
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                            if ((firmask & (mshift >> 8)) or (secmask & (mshift >> 8)))
                            {
                                uint64_t shiftconstsec = (mshift >> 9);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords - 9);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] -= 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords - 9);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] -= 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] += 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            alpha = reschild;
                                        }
                                    }
                                }
                            }
                            if ((firmask & (mshift << 8)) or (secmask & (mshift << 8)))
                            {
                                uint64_t shiftconstsec = (mshift << 7);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 7);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 7);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] -= 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
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
                }
                if (mshift << 8)
                {
                    uint64_t shiftconst = (mshift << 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firlink;
                                temp.secl ^= shiftconst;
                                removesecondlink(temp, coords + 8);
                                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                if (i == 0)
                                    temp.firl ^= (mshift | shiftconst);
                                else
                                    temp.firv ^= (mshift | shiftconst);
                                ++temp.firvirus;
                                temp.secv ^= shiftconst;
                                removesecondvirus(temp, coords + 8);
                                int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                        }
                        else
                        {
                            position.fir[i] += 8;
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                            position.fir[i] -= 8;
                            if (i == 0)
                                position.firl ^= (mshift | shiftconst);
                            else
                                position.firv ^= (mshift | shiftconst);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                            if (x < 7)
                            {
                                uint64_t shiftconstsec = (mshift << 9);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 9);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 9;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 9);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] -= 9;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            alpha = reschild;
                                        }
                                    }
                                }
                            }
                            if (x > 0)
                            {
                                uint64_t shiftconstsec = (mshift << 7);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 7);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 7;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 7);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] -= 7;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            alpha = reschild;
                                        }
                                    }
                                }
                            }
                            if (mshift << 16)
                            {
                                uint64_t shiftconstsec = (mshift << 16);
                                if ((firmask & shiftconstsec) == 0)
                                {
                                    if (secmask & shiftconstsec)
                                    {
                                        if (position.secl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 16;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firlink;
                                            temp.secl ^= shiftconstsec;
                                            removesecondlink(temp, coords + 16);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                        else if (position.firvirus < 3)
                                        {
                                            field temp = position;
                                            temp.fir[i] += 16;
                                            if (i == 0)
                                                temp.firl ^= (mshift | shiftconstsec);
                                            else
                                                temp.firv ^= (mshift | shiftconstsec);
                                            ++temp.firvirus;
                                            temp.secv ^= shiftconstsec;
                                            removesecondvirus(temp, coords + 16);
                                            int reschild = minimax(depth, alpha, beta, false, temp, cache, terminate);
                                            if (reschild > alpha)
                                            {
                                                if (beta <= reschild)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                alpha = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.fir[i] += 16;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, false, position, cache, terminate);
                                        position.fir[i] -= 16;
                                        if (i == 0)
                                            position.firl ^= (mshift | shiftconstsec);
                                        else
                                            position.firv ^= (mshift | shiftconstsec);
                                        if (reschild > alpha)
                                        {
                                            if (beta <= reschild)
                                            {
                                                if (depth > mincachedepth)
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
                }
            }
            __builtin_assume(position.firvirusindex >= 4 and position.firvirusindex <= 8);
            for (int i = 4 + ((position.fir[4] >> 6) & 1); i < position.firvirusindex; ++i)
            {
                const int coords = position.fir[i], x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                uint64_t shiftconst;
                if (mshift >> 8)
                {
                    shiftconst = (mshift >> 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords - 8);
                                int reschild;
                                minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords - 8);
                                int reschild;
                                minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
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
                            position.fir[i] -= 8;
                            position.firv ^= (shiftconst | mshift);
                            minimaxscoutsec(reschild, depth, position, alpha, beta, cache, terminate);
                            position.fir[i] += 8;
                            position.firv ^= (shiftconst | mshift);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                }
                if (x < 7)
                {
                    shiftconst = (mshift << 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords + 1);
                                int reschild;
                                minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords + 1);
                                int reschild;
                                minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
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
                            ++position.fir[i];
                            position.firv ^= (shiftconst | mshift);
                            minimaxscoutsec(reschild, depth, position, alpha, beta, cache, terminate);
                            --position.fir[i];
                            position.firv ^= (shiftconst | mshift);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                }
                if (x > 0)
                {
                    shiftconst = (mshift >> 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                --temp.fir[i];
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords - 1);
                                int reschild;
                                minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                --temp.fir[i];
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords - 1);
                                int reschild;
                                minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
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
                            --position.fir[i];
                            position.firv ^= (shiftconst | mshift);
                            minimaxscoutsec(reschild, depth, position, alpha, beta, cache, terminate);
                            ++position.fir[i];
                            position.firv ^= (shiftconst | mshift);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                }
                if (mshift << 8)
                {
                    shiftconst = (mshift << 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords + 8);
                                int reschild;
                                minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                temp.firv ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords + 8);
                                int reschild;
                                minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
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
                            position.fir[i] += 8;
                            position.firv ^= (shiftconst | mshift);
                            minimaxscoutsec(reschild, depth, position, alpha, beta, cache, terminate);
                            position.fir[i] -= 8;
                            position.firv ^= (shiftconst | mshift);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                }
            }
            __builtin_assume(position.firlinkindex >= 0 and position.firlinkindex <= 4);
            for (int i = ((position.fir[0] >> 6) & 1); i < position.firlinkindex; ++i)
            {
                const int coords = position.fir[i], x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                uint64_t shiftconst;
                if (mshift >> 8)
                {
                    shiftconst = (mshift >> 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords - 8);
                                int reschild;
                                minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] -= 8;
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords - 8);
                                int reschild;
                                minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
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
                            position.fir[i] -= 8;
                            position.firl ^= (shiftconst | mshift);
                            minimaxscoutsec(reschild, depth, position, alpha, beta, cache, terminate);
                            position.fir[i] += 8;
                            position.firl ^= (shiftconst | mshift);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                }
                if (x < 7)
                {
                    shiftconst = (mshift << 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords + 1);
                                int reschild;
                                minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                ++temp.fir[i];
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords + 1);
                                int reschild;
                                minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
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
                            ++position.fir[i];
                            position.firl ^= (shiftconst | mshift);
                            minimaxscoutsec(reschild, depth, position, alpha, beta, cache, terminate);
                            --position.fir[i];
                            position.firl ^= (shiftconst | mshift);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                }
                if (x > 0)
                {
                    shiftconst = (mshift >> 1);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                --temp.fir[i];
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords - 1);
                                int reschild;
                                minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                --temp.fir[i];
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords - 1);
                                int reschild;
                                minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
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
                            --position.fir[i];
                            position.firl ^= (shiftconst | mshift);
                            minimaxscoutsec(reschild, depth, position, alpha, beta, cache, terminate);
                            ++position.fir[i];
                            position.firl ^= (shiftconst | mshift);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                alpha = reschild;
                            }
                        }
                    }
                }
                if (mshift << 8)
                {
                    shiftconst = (mshift << 8);
                    if ((firmask & shiftconst) == 0)
                    {
                        if (secmask & shiftconst)
                        {
                            if (position.secl & shiftconst)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firlink;
                                temp.secl ^= (shiftconst);
                                removesecondlink(temp, coords + 8);
                                int reschild;
                                minimaxfullsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    alpha = reschild;
                                }
                            }
                            else if (position.firvirus < 3)
                            {
                                field temp = position;
                                temp.fir[i] += 8;
                                temp.firl ^= (shiftconst | mshift);
                                ++temp.firvirus;
                                temp.secv ^= (shiftconst);
                                removesecondvirus(temp, coords + 8);
                                int reschild;
                                minimaxscoutsec(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (reschild > alpha)
                                {
                                    if (beta <= reschild)
                                    {
                                        if (depth > mincachedepth)
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
                            position.fir[i] += 8;
                            position.firl ^= (shiftconst | mshift);
                            minimaxscoutsec(reschild, depth, position, alpha, beta, cache, terminate);
                            position.fir[i] -= 8;
                            position.firl ^= (shiftconst | mshift);
                            if (reschild > alpha)
                            {
                                if (beta <= reschild)
                                {
                                    if (depth > mincachedepth)
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
        {
            // todo
        }
        if (depth > mincachedepthfull and terminate == false)
            cache[depth][position] = {alpha, (alpha > alphabeg) ? 3 : 1};
        return alpha;
    }
    else
    {
        if (terminate)
            return alpha;
        if (depth == 0)
            return position.evaluatesec();
        --depth;
        if (position.seclink == 3)
        {
            if (position.secl & 1729382256910270464ULL)
                return (-16384 * depth);
            if (position.firl)
            {
                const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
                if (((position.firl >> 8) & secmask) or ((position.firl << 8) & secmask))
                    return (-16384 * depth);
                __builtin_assume(position.firlinkindex > 0 and position.firlinkindex <= 4);
#pragma clang loop unroll_count(4)
                for (int i = 0; i < position.firlinkindex; ++i)
                {
                    const int cachedpos = position.fir[i], t = (cachedpos & 7);
                    if (t < 7)
                        if (secmask & (1ULL << (cachedpos + 1)))
                            return (-16384 * depth);
                    if (t > 0)
                        if (secmask & (1ULL << (cachedpos - 1)))
                            return (-16384 * depth);
                }
                if (position.sec[4] & 64)
                {
                    const int cachedpos = (position.sec[4] & 63), t = (cachedpos & 7);
                    if (cachedpos < 56)
                    {
                        if (cachedpos < 48)
                            if (position.firl & (1ULL << (cachedpos + 16)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (-16384 * depth);
                        if (t < 7)
                            if (position.firl & (1ULL << (cachedpos + 9)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos + 1))) == 0 or ((position.firv | secmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (-16384 * depth);
                        if (t > 0)
                            if (position.firl & (1ULL << (cachedpos + 7)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos - 1))) == 0 or ((position.firv | secmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (-16384 * depth);
                    }
                    if (cachedpos > 7)
                    {
                        if (cachedpos > 15)
                            if (position.firl & (1ULL << (cachedpos - 16)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (-16384 * depth);
                        if (t < 7)
                            if (position.firl & (1ULL << (cachedpos - 7)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos + 1))) == 0 or ((position.firv | secmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (-16384 * depth);
                        if (t > 0)
                            if (position.firl & (1ULL << (cachedpos - 9)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos - 1))) == 0 or ((position.firv | secmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (-16384 * depth);
                    }
                    if (t < 6)
                    {
                        if (position.firl & (1ULL << (cachedpos + 2)))
                            if (((position.firv | secmask) & (1ULL << (cachedpos + 1))) == 0)
                                return (-16384 * depth);
                    }
                    if (t > 1)
                    {
                        if (position.firl & (1ULL << (cachedpos - 2)))
                            if (((position.firv | secmask) & (1ULL << (cachedpos - 1))) == 0)
                                return (-16384 * depth);
                    }
                }
                if (position.sec[0] & 64)
                {
                    const int cachedpos = (position.sec[0] & 63), t = (cachedpos & 7);
                    // if(cachedpos == 58 or cachedpos == 59 or cachedpos == 60 or cachedpos == 61 or (cachedpos == 52 and ((firmask | secmask) & (1ULL << 60)) == 0) or (cachedpos == 51 and ((firmask | secmask) & (1ULL << 61)) == 0))
                    //     return (-16384 * depth);
                    if (cachedpos < 56)
                    {
                        if (cachedpos < 48)
                            if (position.firl & (1ULL << (cachedpos + 16)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (-16384 * depth);
                        if (t < 7)
                            if (position.firl & (1ULL << (cachedpos + 9)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos + 1))) == 0 or ((position.firv | secmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (-16384 * depth);
                        if (t > 0)
                            if (position.firl & (1ULL << (cachedpos + 7)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos - 1))) == 0 or ((position.firv | secmask) & (1ULL << (cachedpos + 8))) == 0)
                                    return (-16384 * depth);
                    }
                    if (cachedpos > 7)
                    {
                        if (cachedpos > 15)
                            if (position.firl & (1ULL << (cachedpos - 16)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (-16384 * depth);
                        if (t < 7)
                            if (position.firl & (1ULL << (cachedpos - 7)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos + 1))) == 0 or ((position.firv | secmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (-16384 * depth);
                        if (t > 0)
                            if (position.firl & (1ULL << (cachedpos - 9)))
                                if (((position.firv | secmask) & (1ULL << (cachedpos - 1))) == 0 or ((position.firv | secmask) & (1ULL << (cachedpos - 8))) == 0)
                                    return (-16384 * depth);
                    }
                    if (t < 6)
                        if (position.firl & (1ULL << (cachedpos + 2)))
                            if (((position.firv | secmask) & (1ULL << (cachedpos + 1))) == 0)
                                return (-16384 * depth);
                    if (t > 1)
                        if (position.firl & (1ULL << (cachedpos - 2)))
                            if (((position.firv | secmask) & (1ULL << (cachedpos - 1))) == 0)
                                return (-16384 * depth);
                }
            }
        }
        int betabeg;
        if (depth > mincachedepth)
        {
            auto it = cache[depth].find(position);
            if (it != cache[depth].end())
            {
                ttentry entry = it->second;
                if (__builtin_expect(entry.flag & 1, 0))
                {
                    if (entry.score >= beta) // if current beta <= cached beta then the beta during evaluation wont change, thus we can return the current beta
                        return beta;
                    if (entry.flag > 1) // if the cached beta is exact and it is smaller than the current beta (because of the condition above) then we can return it
                        return entry.score;
                    alpha = max(alpha, entry.score);
                    // cached beta is upper bound
                }
                else
                {
                    if (entry.score < beta)
                    {
                        if (entry.score <= alpha)
                            return entry.score;
                        beta = entry.score;
                    }
                }
            }
            betabeg = beta;
        }
        if (__builtin_expect(position.secl & 1729382256910270464ULL, 0))
        {
            __builtin_assume(position.seclinkindex > 0 and position.seclinkindex < 5);
#pragma clang loop unroll_count(4)
            for (auto i = 0; i < position.seclinkindex; ++i)
            {
                const int t2 = (position.sec[i] & 63);
                if (t2 == 59 or t2 == 60)
                {
                    field temp = position;
                    if (position.sec[i] & 64)
                        temp.isboostavailablesec = true;
                    --temp.seclinkindex;
                    temp.sec[i] = temp.sec[temp.seclinkindex];
                    temp.sec[temp.seclinkindex] = 0;
                    temp.secl ^= (1ULL << t2);
                    ++temp.seclink;
                    int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                    if (beta > reschild)
                    {
                        if (reschild <= alpha)
                        {
                            if (depth > mincachedepth)
                                cache[depth][position] = {reschild, 0};
                            return reschild;
                        }
                        beta = reschild;
                    }
                }
            }
        }
        if (position.isboostavailablesec)
        {
            __builtin_assume(position.secvirusindex >= 4 and position.secvirusindex <= 8);
#pragma clang loop unroll_count(4)
            for (auto i = 4; i < position.secvirusindex; ++i)
            {
                position.sec[i] ^= 64;
                position.isboostavailablesec = false;
                int reschild;
                if (i > 4)
                {
                    swap(position.sec[i], position.sec[4]);
                    reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                    swap(position.sec[i], position.sec[4]);
                }
                else
                    reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                position.sec[i] ^= 64;
                position.isboostavailablesec = true;
                if (beta > reschild)
                {
                    if (reschild <= alpha)
                    {
                        if (depth > mincachedepth)
                            cache[depth][position] = {reschild, 0};
                        return reschild;
                    }
                    beta = reschild;
                }
            }
            __builtin_assume(position.seclinkindex >= 0 and position.seclinkindex <= 4);
#pragma clang loop unroll_count(4)
            for (auto i = 0; i < position.seclinkindex; ++i)
            {
                position.sec[i] ^= 64;
                position.isboostavailablesec = false;
                int reschild;
                if (i > 0)
                {
                    swap(position.sec[i], position.sec[0]);
                    reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                    swap(position.sec[i], position.sec[0]);
                }
                else
                    reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                position.sec[i] ^= 64;
                position.isboostavailablesec = true;
                if (beta > reschild)
                {
                    if (reschild <= alpha)
                    {
                        if (depth > mincachedepth)
                            cache[depth][position] = {reschild, 0};
                        return reschild;
                    }
                    beta = reschild;
                }
            }
        }
        const uint64_t firmask = (position.firl | position.firv), secmask = (position.secl | position.secv);
        if (firewallfir < 0)
        {
            if (position.isboostavailablesec == false)
            {
                int i;
                if (position.sec[0] & 64)
                    i = 0;
                else
                    i = 4;
                const int coords = (position.sec[i] & 63), x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                if (mshift << 8)
                {
                    uint64_t shiftconst = (mshift << 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.seclink;
                                temp.firl ^= shiftconst;
                                removefirstlink(temp, coords + 8);
                                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.secvirus;
                                temp.firv ^= shiftconst;
                                removefirstvirus(temp, coords + 8);
                                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                        }
                        else
                        {
                            if (mshift << 16)
                            {
                                uint64_t shiftconstsec = (mshift << 16);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 16;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 16);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 16;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 16);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 16;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] -= 16;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            beta = reschild;
                                        }
                                    }
                                }
                            }
                            position.sec[i] += 8;
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                            position.sec[i] -= 8;
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                            if (x > 0)
                            {
                                uint64_t shiftconstsec = (mshift << 7);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 7);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 7);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] -= 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            beta = reschild;
                                        }
                                    }
                                }
                            }
                            if (x < 7)
                            {
                                uint64_t shiftconstsec = (mshift << 9);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 9);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 9);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] -= 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
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
                }
                if (x > 0)
                {
                    uint64_t shiftconst = (mshift >> 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                --temp.sec[i];
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.seclink;
                                temp.firl ^= shiftconst;
                                removefirstlink(temp, coords - 1);
                                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                --temp.sec[i];
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.secvirus;
                                temp.firv ^= shiftconst;
                                removefirstvirus(temp, coords - 1);
                                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                        }
                        else
                        {
                            if (x > 1)
                            {
                                uint64_t shiftconstsec = (mshift >> 2);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 2;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 2);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 2;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 2);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 2;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] += 2;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            beta = reschild;
                                        }
                                    }
                                }
                            }
                            --position.sec[i];
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                            ++position.sec[i];
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                            if ((secmask & (mshift << 8)) or (firmask & (mshift << 8)))
                            {
                                uint64_t shiftconstsec = (mshift << 7);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 7);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 7);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] -= 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            beta = reschild;
                                        }
                                    }
                                }
                            }
                            if ((secmask & (mshift >> 8)) or (firmask & (mshift >> 8)))
                            {
                                uint64_t shiftconstsec = (mshift >> 9);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 9);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 9);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] += 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
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
                }
                if (x < 7)
                {
                    uint64_t shiftconst = (mshift << 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.seclink;
                                temp.firl ^= shiftconst;
                                removefirstlink(temp, coords + 1);
                                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.secvirus;
                                temp.firv ^= shiftconst;
                                removefirstvirus(temp, coords + 1);
                                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                        }
                        else
                        {
                            if (x < 6)
                            {
                                uint64_t shiftconstsec = (mshift << 2);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 2;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 2);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 2;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 2);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 2;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] -= 2;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            beta = reschild;
                                        }
                                    }
                                }
                            }
                            ++position.sec[i];
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                            --position.sec[i];
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                            if ((secmask & (mshift << 8)) or (firmask & (mshift << 8)))
                            {
                                uint64_t shiftconstsec = (mshift << 9);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords + 9);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] += 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords + 9);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] += 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] -= 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            beta = reschild;
                                        }
                                    }
                                }
                            }
                            if ((secmask & (mshift >> 8)) or (firmask & (mshift >> 8)))
                            {
                                uint64_t shiftconstsec = (mshift >> 7);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 7);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 7);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] += 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
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
                }
                if (mshift >> 8)
                {
                    uint64_t shiftconst = (mshift >> 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.seclink;
                                temp.firl ^= shiftconst;
                                removefirstlink(temp, coords - 8);
                                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                if (i == 0)
                                    temp.secl ^= (mshift | shiftconst);
                                else
                                    temp.secv ^= (mshift | shiftconst);
                                ++temp.secvirus;
                                temp.firv ^= shiftconst;
                                removefirstvirus(temp, coords - 8);
                                int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                        }
                        else
                        {
                            position.sec[i] -= 8;
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                            position.sec[i] += 8;
                            if (i == 0)
                                position.secl ^= (mshift | shiftconst);
                            else
                                position.secv ^= (mshift | shiftconst);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                            if (x > 0)
                            {
                                uint64_t shiftconstsec = (mshift >> 9);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 9);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 9;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 9);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] += 9;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            beta = reschild;
                                        }
                                    }
                                }
                            }
                            if (x < 7)
                            {
                                uint64_t shiftconstsec = (mshift >> 7);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 7);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 7;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 7);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] += 7;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
                                                    cache[depth][position] = {reschild, 0};
                                                return reschild;
                                            }
                                            beta = reschild;
                                        }
                                    }
                                }
                            }
                            if (mshift >> 16)
                            {
                                uint64_t shiftconstsec = (mshift >> 16);
                                if ((secmask & shiftconstsec) == 0)
                                {
                                    if (firmask & shiftconstsec)
                                    {
                                        if (position.firl & shiftconstsec)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 16;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.seclink;
                                            temp.firl ^= shiftconstsec;
                                            removefirstlink(temp, coords - 16);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                        else if (position.secvirus < 3)
                                        {
                                            field temp = position;
                                            temp.sec[i] -= 16;
                                            if (i == 0)
                                                temp.secl ^= (mshift | shiftconstsec);
                                            else
                                                temp.secv ^= (mshift | shiftconstsec);
                                            ++temp.secvirus;
                                            temp.firv ^= shiftconstsec;
                                            removefirstvirus(temp, coords - 16);
                                            int reschild = minimax(depth, alpha, beta, true, temp, cache, terminate);
                                            if (beta > reschild)
                                            {
                                                if (reschild <= alpha)
                                                {
                                                    if (depth > mincachedepth)
                                                        cache[depth][position] = {reschild, 0};
                                                    return reschild;
                                                }
                                                beta = reschild;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        position.sec[i] -= 16;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        int reschild = minimax(depth, alpha, beta, true, position, cache, terminate);
                                        position.sec[i] += 16;
                                        if (i == 0)
                                            position.secl ^= (mshift | shiftconstsec);
                                        else
                                            position.secv ^= (mshift | shiftconstsec);
                                        if (beta > reschild)
                                        {
                                            if (reschild <= alpha)
                                            {
                                                if (depth > mincachedepth)
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
                }
            }
            __builtin_assume(position.secvirusindex >= 4 and position.secvirusindex <= 8);
            for (auto i = 4 + ((position.sec[4] >> 6) & 1); i < position.secvirusindex; ++i)
            {
                const int coords = position.sec[i], x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                uint64_t shiftconst;
                if (mshift << 8)
                {
                    shiftconst = (mshift << 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords + 8);
                                int reschild;
                                minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords + 8);
                                int reschild;
                                minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
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
                            position.sec[i] += 8;
                            position.secv ^= (shiftconst | mshift);
                            minimaxscoutfir(reschild, depth, position, alpha, beta, cache, terminate);
                            position.sec[i] -= 8;
                            position.secv ^= (shiftconst | mshift);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                }
                if (x > 0)
                {
                    shiftconst = (mshift >> 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                --temp.sec[i];
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords - 1);
                                int reschild;
                                minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                --temp.sec[i];
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords - 1);
                                int reschild;
                                minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
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
                            --position.sec[i];
                            position.secv ^= (shiftconst | mshift);
                            minimaxscoutfir(reschild, depth, position, alpha, beta, cache, terminate);
                            ++position.sec[i];
                            position.secv ^= (shiftconst | mshift);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                }
                if (x < 7)
                {
                    shiftconst = (mshift << 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords + 1);
                                int reschild;
                                minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords + 1);
                                int reschild;
                                minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
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
                            ++position.sec[i];
                            position.secv ^= (shiftconst | mshift);
                            minimaxscoutfir(reschild, depth, position, alpha, beta, cache, terminate);
                            --position.sec[i];
                            position.secv ^= (shiftconst | mshift);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                }
                if (mshift >> 8)
                {
                    shiftconst = (mshift >> 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords - 8);
                                int reschild;
                                minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                temp.secv ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords - 8);
                                int reschild;
                                minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
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
                            position.sec[i] -= 8;
                            position.secv ^= (shiftconst | mshift);
                            minimaxscoutfir(reschild, depth, position, alpha, beta, cache, terminate);
                            position.sec[i] += 8;
                            position.secv ^= (shiftconst | mshift);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                }
            }
            __builtin_assume(position.seclinkindex >= 0 and position.seclinkindex <= 4);
            for (auto i = ((position.sec[0] >> 6) & 1); i < position.seclinkindex; ++i)
            {
                const int coords = position.sec[i], x = (coords & 7);
                const uint64_t mshift = (1ULL << coords);
                uint64_t shiftconst;
                if (mshift << 8)
                {
                    shiftconst = (mshift << 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords + 8);
                                int reschild;
                                minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] += 8;
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords + 8);
                                int reschild;
                                minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
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
                            position.sec[i] += 8;
                            position.secl ^= (shiftconst | mshift);
                            minimaxscoutfir(reschild, depth, position, alpha, beta, cache, terminate);
                            position.sec[i] -= 8;
                            position.secl ^= (shiftconst | mshift);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                }
                if (x > 0)
                {
                    shiftconst = (mshift >> 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                --temp.sec[i];
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords - 1);
                                int reschild;
                                minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                --temp.sec[i];
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords - 1);
                                int reschild;
                                minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
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
                            --position.sec[i];
                            position.secl ^= (shiftconst | mshift);
                            minimaxscoutfir(reschild, depth, position, alpha, beta, cache, terminate);
                            ++position.sec[i];
                            position.secl ^= (shiftconst | mshift);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                }
                if (x < 7)
                {
                    shiftconst = (mshift << 1);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords + 1);
                                int reschild;
                                minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                ++temp.sec[i];
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords + 1);
                                int reschild;
                                minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
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
                            ++position.sec[i];
                            position.secl ^= (shiftconst | mshift);
                            minimaxscoutfir(reschild, depth, position, alpha, beta, cache, terminate);
                            --position.sec[i];
                            position.secl ^= (shiftconst | mshift);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
                                        cache[depth][position] = {reschild, 0};
                                    return reschild;
                                }
                                beta = reschild;
                            }
                        }
                    }
                }
                if (mshift >> 8)
                {
                    shiftconst = (mshift >> 8);
                    if ((secmask & shiftconst) == 0)
                    {
                        if (firmask & shiftconst)
                        {
                            if (position.firl & shiftconst)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.seclink;
                                temp.firl ^= (shiftconst);
                                removefirstlink(temp, coords - 8);
                                int reschild;
                                minimaxfullfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
                                            cache[depth][position] = {reschild, 0};
                                        return reschild;
                                    }
                                    beta = reschild;
                                }
                            }
                            else if (position.secvirus < 3)
                            {
                                field temp = position;
                                temp.sec[i] -= 8;
                                temp.secl ^= (shiftconst | mshift);
                                ++temp.secvirus;
                                temp.firv ^= (shiftconst);
                                removefirstvirus(temp, coords - 8);
                                int reschild;
                                minimaxscoutfir(reschild, depth, temp, alpha, beta, cache, terminate);
                                if (beta > reschild)
                                {
                                    if (reschild <= alpha)
                                    {
                                        if (depth > mincachedepth)
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
                            position.sec[i] -= 8;
                            position.secl ^= (shiftconst | mshift);
                            minimaxscoutfir(reschild, depth, position, alpha, beta, cache, terminate);
                            position.sec[i] += 8;
                            position.secl ^= (shiftconst | mshift);
                            if (beta > reschild)
                            {
                                if (reschild <= alpha)
                                {
                                    if (depth > mincachedepth)
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
        {
            // todo
        }
        if (depth > mincachedepthfull and terminate == false)
            cache[depth][position] = {beta, (beta < betabeg) ? 3 : 1};
        return beta;
    }
}

const int multifinish = 2;

mutex mtx;

void displayProgressBar(const double total, const double finished, const string &text)
{
    cout << "\33[2K\r" << flush;
    cout << text << " " << int(finished * 100.0 / total) << " %\r" << flush;
}

int cutoffdepth;

int minimaxscout(const int depth, int alpha, int beta, const bool player, field &position)
{
    if (depth < cutoffdepth)
    {
        bool toterminate = false;
        vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
        return minimax(depth, alpha, beta, player, position, newcache, toterminate);
    }
    if (player)
    {
        vector<field> allmoves = possiblemoves(position, true);
        for (int i = 0; i < allmoves.size(); ++i)
            if (allmoves[i].firlink > 3)
                return (16384 * depth);
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]()
                                {
                bool toterminate = false;
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                scores[i] = minimax(depth - 3, alpha, beta, false, allmoves[i], newcache, toterminate);
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2a/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        int max = scores[0], index = 0;
        for (int i = 1; i < allmoves.size(); ++i)
        {
            if (scores[i] > max)
            {
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
        // cout << endl;
        // cout << "D" << depth << " alpha: " << alpha << endl;
        auto start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished, &toterminate, &tscore]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
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
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2b/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        // cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        if (toterminate)
            return tscore;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            if (scores[i] > alpha)
            {
                alpha = scores[i];
            }
        }
        return alpha;
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for (int i = 0; i < allmoves.size(); ++i)
            if (allmoves[i].seclink > 3)
                return (-16384 * depth);
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]()
                                {
                bool toterminate = false;
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                scores[i] = minimax(depth - 3, alpha, beta, true, allmoves[i], newcache, toterminate);
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2a/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        int min = scores[0], index = 0;
        for (int i = 1; i < allmoves.size(); ++i)
        {
            if (scores[i] < min)
            {
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
        // cout << endl;
        // cout << "D" << depth << " beta: " << beta << endl;
        auto start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished, &toterminate, &tscore]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
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
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2b/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        // cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        if (toterminate)
            return tscore;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            if (beta > scores[i])
            {
                beta = scores[i];
            }
        }
        return beta;
    }
}

pair<field, int> minimaxmain(const int depth, int alpha, int beta, const bool player, field &position)
{
    cutoffdepth = depth - 5;
    if (player)
    {
        vector<field> allmoves = possiblemoves(position, true);
        for (int i = 0; i < allmoves.size(); ++i)
            if (allmoves[i].firlink > 3)
                return make_pair(allmoves[i], (16384 * depth));
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        auto start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]()
                                {
                bool toterminate = false;
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 3, alpha, beta, false, allmoves[i], newcache, toterminate);
                auto stop = high_resolution_clock::now();
                //cout << "Predict time: " << duration_cast<milliseconds>(stop - start).count() << endl;
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 1/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        int max = scores[0], index = 0;
        for (int i = 1; i < allmoves.size(); ++i)
        {
            if (scores[i] > max)
            {
                max = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        // cout << "Prediction time: " << duration_cast<milliseconds>(end - start).count() << endl;
        field bestfield = allmoves[0];
        alpha = minimaxscout(depth - 1, alpha, beta, false, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        // cout << endl;
        // cout << "alpha: " << alpha << endl;
        bool toterminate = false;
        start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished, &toterminate]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
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
                displayProgressBar(allmoves.size(), finished, "Calculating stage 3/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        end = high_resolution_clock::now();
        // cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            if (scores[i] > alpha)
            {
                alpha = scores[i];
                bestfield = allmoves[i];
            }
        }
        return make_pair(bestfield, alpha);
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for (int i = 0; i < allmoves.size(); ++i)
            if (allmoves[i].seclink > 3)
                return make_pair(allmoves[i], (-16384 * depth));
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        auto start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]()
                                {
                bool toterminate = false;
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 3, alpha, beta, true, allmoves[i], newcache, toterminate);
                auto stop = high_resolution_clock::now();
                //cout << "Predict time: " << duration_cast<milliseconds>(stop - start).count() << endl;
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 1/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        int min = scores[0], index = 0;
        for (int i = 1; i < allmoves.size(); ++i)
        {
            if (scores[i] < min)
            {
                min = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        // cout << "Prediction time: " << duration_cast<milliseconds>(end - start).count() << endl;
        field bestfield = allmoves[0];
        beta = minimaxscout(depth - 1, alpha, beta, true, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        // cout << endl;
        // cout << "beta: " << beta << endl;
        bool toterminate = false;
        start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, &alpha, &beta, &allmoves, &finished, &toterminate]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
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
                // if(toterminate){
                //     mtx.unlock();
                //     scores[i] = minimaxscout(depth - 1, alpha, beta, true, allmoves[i]);
                // }
                // else if(allmoves.size() - curfreethreads == multifinish and duration_cast<milliseconds>(stop - start).count() > (800 * (1 << (depth - 12))) and depth > 11){
                //     toterminate = true;
                // }
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 3/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        end = high_resolution_clock::now();
        // cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            if (beta > scores[i])
            {
                beta = scores[i];
                bestfield = allmoves[i];
            }
        }
        return make_pair(bestfield, beta);
    }
}

inline bool isunique(field &toadd, vector<field> &array)
{
    for (int i = 0; i < array.size(); ++i)
        if (toadd == array[i])
            return false;
    return true;
}

int main()
{
    // srand(time(NULL));
    field pos;
    pair<field, int> move;
    // auto start = high_resolution_clock::now();
    // int checksum = 0;
    // for (int i = 0; i < 70; ++i)
    // {
    //     int fir, sec;
    //     generatexfield(pos, true, indexes[69]);
    //     generatexfield(pos, false, indexes[i]);
    //     auto startit = high_resolution_clock::now();
    //     move = minimaxmain(12, MIN, MAX, true, pos);
    //     auto endit = high_resolution_clock::now();
    //     cout << "\33[2K\r" << flush;
    //     cout << move.second << "     " << duration_cast<milliseconds>(endit - startit).count() << endl;
    //     fir = move.second;
    //     checksum ^= (move.second << (indexes[i] & 1));
    //     generatexfield(pos, true, indexes[i]);
    //     generatexfield(pos, false, indexes[69]);
    //     startit = high_resolution_clock::now();
    //     move = minimaxmain(12, MIN, MAX, false, pos);
    //     endit = high_resolution_clock::now();
    //     cout << "\33[2K\r" << flush;
    //     cout << move.second << "     " << duration_cast<milliseconds>(endit - startit).count() << endl;
    //     sec = move.second;
    //     if(fir != -1 * sec){
    //         cout << "Eval error\n";
    //         exit(1);
    //     }
    //     checksum ^= (move.second << (indexes[i] & 1));
    // }
    // auto end = high_resolution_clock::now();
    // cout << duration_cast<milliseconds>(end - start).count() << endl;
    // cout << "hash: " << checksum << endl;
    // return 0;
    generatexfield(pos, true, indexes[0]);
    generatexfield(pos, false, indexes[0]);
    printfield(pos);
    cout << endl;
    auto startm = high_resolution_clock::now();
    for (;;)
    {
        auto start = high_resolution_clock::now();
        move = minimaxmain(12, MIN, MAX, false, pos);
        auto end = high_resolution_clock::now();
        cout << "\33[2K\r" << flush;
        cout << "Minimized score: " << move.second << "      " << duration_cast<milliseconds>(end - start).count() << endl;
        pos = move.first;
        printfield(pos);
        cout << endl;
        if (pos.seclink == 4)
        {
            cout << "Player one wins! " << endl;
            printfield(pos);
            break;
        }
        else if (pos.secvirus == 4)
        {
            cout << "Player one loses! " << endl;
            printfield(pos);
            break;
        }
        start = high_resolution_clock::now();
        move = minimaxmain(12, MIN, MAX, true, pos);
        end = high_resolution_clock::now();
        cout << "\33[2K\r" << flush;
        cout << "Maximized score: " << move.second << "      " << duration_cast<milliseconds>(end - start).count() << endl;
        pos = move.first;
        printfield(pos);
        cout << endl;
        if (pos.firlink == 4)
        {
            cout << "Player two wins! " << endl;
            printfield(pos);
            break;
        }
        else if (pos.firvirus == 4)
        {
            cout << "Player two loses! " << endl;
            printfield(pos);
            break;
        }
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
