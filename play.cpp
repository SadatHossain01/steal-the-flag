#pragma GCC optimize("O3,inline")
#pragma GCC target("bmi,bmi2,lzcnt,popcnt")

#include <algorithm>
#include <bitset>
#include <cassert>
#include <iostream>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <vector>
using namespace std;

typedef long long ll;

struct Powerup;
struct Game;

int height, width, n_minions = -1;
const int INF = 5000;
bool moveDone;          // make sure no minion move is wasted?
bool firstTime = true;  // for last state of the game
vector<string> grid;
vector<pair<int, int>> not_visited;
set<int> explorerMinions;
int myFlagBaseX, myFlagBaseY, oppFlagBaseX, oppFlagBaseY;
// this is for different calls of bfs()
vector<vector<bool>> visited;
vector<int> assignedExploration;
vector<vector<pair<int, int>>> cellsToExplore;
vector<vector<bool>> leftToVisit;  // for exploration
// distOptional will be used for miscellaneous purposes
vector<vector<int>> distFromMyBase, distFromOppBase, distOptional;
// for checking if two are in same line, only
// checking x same or y same is not enough
// there can be walls
vector<vector<int>> prefixSumRow, prefixSumCol;
//  vector<vector<pair<int, int>>> parent;
enum opTypes { MOVE, FIRE, FREEZE, MINE, WAIT };

int mandatoryCarrier = -1, flagDefender = -1;

int dx[] = {1, 0, -1, 0};
int dy[] = {0, 1, 0, -1};

inline void ign() { cin.ignore(); }

struct Powerup {
    string name;
    int price, damage;
    inline void readPowerUp()
    {
        cin >> name >> price >> damage;
        ign();
    }
};

Powerup powerups[3];  // 0 fire, 1 freeze, 2 mine

struct Coin {
    int posX, posY;
    bool taken;
    inline void readCoin()
    {
        cin >> posX >> posY;
        ign();
        taken = false;
    }
};

vector<Coin> assignedCoins;  // explorer minion দের closest coin টা assign করে দেয়া হবে, যদি থাকে

struct Minion {
    // int id; this is basically the array index
    int posX, posY, health, timeout;
    // read the id first, and then call minion[id].readMinion()
    inline void readMinion()
    {
        cin >> posX >> posY >> health >> timeout;
        ign();
        pair<int, int> p = {posX, posY};
        for (int i = 0; i < n_minions - 2; i++) leftToVisit[posX][posY] = false;
        for (int i = 0; i < n_minions; i++) {
            if (posX == assignedCoins[i].posX &&
                posY == assignedCoins[i].posY) {  //নেয়া হয়ে গেসে coin টা
                assignedCoins[i].posX = -1;
                assignedCoins[i].posY = -1;
            }
        }
    }
};

struct Operation {
    opTypes ty;
    int id, x, y;
    Operation(opTypes o, int id, int x = -1, int y = -1) : ty(o), id(id), x(x), y(y) {}
    void print()
    {
        if (ty == MOVE)
            cout << "MOVE " << id << " " << x << " " << y;
        else if (ty == MINE)
            cout << "MINE " << id << " " << x << " " << y;
        else if (ty == FIRE)
            cout << "FIRE " << id;
        else if (ty == FREEZE)
            cout << "FREEZE " << id;
        else
            cout << "WAIT " << id;
    }
};

void readGrid()
{
    cin >> height >> width;
    ign();
    grid.resize(height);
    for (int i = 0; i < height; i++) {
        string s;
        cin >> s;
        grid[i] = s;
        ign();
    }
    // for O(1) checking if any wall in between
    prefixSumRow.assign(height, vector<int>(width, 0));
    prefixSumCol.assign(height, vector<int>(width, 0));
    // wall 1, normal cell 0
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (col == 0)
                prefixSumRow[row][col] = (grid[row][col] == '#');
            else
                prefixSumRow[row][col] = prefixSumRow[row][col - 1] + (grid[row][col] == '#');
        }
    }
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height; row++) {
            if (row == 0)
                prefixSumCol[row][col] = (grid[row][col] == '#');
            else
                prefixSumCol[row][col] = prefixSumCol[row - 1][col] + (grid[row][col] == '#');
        }
    }
}

void readBases()
{
    cin >> myFlagBaseX >> myFlagBaseY;
    ign();
    cin >> oppFlagBaseX >> oppFlagBaseY;
    ign();
}

void readPowerUps()
{
    for (int i = 0; i < 3; i++) powerups[i].readPowerUp();
}

inline bool isValid(int x, int y)
{
    return x >= 0 && x < height && y >= 0 && y < width && grid[x][y] == '.';
}

inline bool sameLine(int x1, int y1, int x2, int y2, bool wantAdjAnalysis, int lastX, int lastY)
{
    // cerr << "x1: " << x1 << " y1: " << y1 << " x2: " << x2 << " y2: " << y2
    //      << " " << lastX << " " << lastY << endl;
    //  x2 y2 is the target
    bool ret = false;
    int now = -1;
    if (x1 == x2 && y1 == y2) return true;
    // first ensure সে এখানে বসে নাই ইচ্ছা করে, কারণ তা হলে যাওয়ার কয়টা জায়গা আছে
    // matter করে না, বসেই থাকবে যেহেতু
    if (wantAdjAnalysis && (lastX == -1 || (x2 != lastX || y2 != lastY))) {
        // lastX = -1 হলে last এর data নাই
        // so do the direct comparison
        // so dealing with an opponent
        //আমি তাকে যে লাইন ধরে attack করবো, তার যদি ওই লাইন ছেড়ে যাওয়ার ability
        //থাকে, তাহলে ওই attack করে লাভ নাই
        // cerr << "hey " << lastX << " " << lastY << endl;
        if (x1 == x2)
            if (isValid(x2 + 1, y2) || isValid(x2 - 1, y2)) return ret;
        if (y1 == y2)
            if (isValid(x2, y2 + 1) || isValid(x2, y2 - 1)) return ret;
    }
    if (x1 == x2) {  // horizontal matching
        //উল্টা হলে negative আসবে, সমস্যা কী?
        now = prefixSumRow[x2][y2];
        if (y1 != 0) now -= prefixSumRow[x1][y1 - 1];
        if (now == 0) ret = true;
    }
    else if (y1 == y2) {  // vertical matching
        now = prefixSumCol[x2][y2];
        if (x1 != 0) now -= prefixSumCol[x1 - 1][y1];
        if (now == 0) ret = true;
    }
    // cerr << now << " " << (ret ? "true" : "false") << endl;
    return ret;
}

void bfs(pair<int, int> s, vector<vector<int>>& dist)
{
    visited.assign(height, vector<bool>(width, false));
    dist.assign(height, vector<int>(width, INF));
    // parent.assign(height, vector<pair<int, int>>(width, {-1, -1}));
    queue<pair<int, int>> q;

    dist[s.first][s.second] = 0;
    q.push(s);

    while (!q.empty()) {
        pair<int, int> now = q.front();
        q.pop();
        if (visited[now.first][now.second]) continue;
        visited[now.first][now.second] = true;
        vector<int> v{0, 1, 2, 3};
        random_shuffle(v.begin(), v.end());
        for (int dir = 0; dir < 4; dir++) {
            int newX = dx[v[dir]] + now.first;
            int newY = dy[v[dir]] + now.second;

            if (!isValid(newX, newY) || visited[newX][newY]) continue;
            dist[newX][newY] = dist[now.first][now.second] + 1;
            // parent[newX][newY] = {now.first, now.second};
            q.push({newX, newY});
        }
    }
}

void dfs(int x, int y)
{
    visited[x][y] = true;
    not_visited.push_back({x, y});
    vector<int> v{0, 1, 2, 3};
    random_shuffle(v.begin(), v.end());
    for (int dir = 0; dir < 4; dir++) {
        int newX = dx[v[dir]] + x;
        int newY = dy[v[dir]] + y;
        if (!isValid(newX, newY) || visited[newX][newY]) continue;
        dfs(newX, newY);
    }
}

void init()
{
    // explorer minions will just explore for more and more coins
    int startX, startY;
    //না, start কোনো এক কোণা থেকে শুরু করলে ভালো মনেহয়
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (!visited[i][j]) continue;
            startX = i;
            startY = j;
            break;
        }
    }
    visited.assign(height, vector<bool>(width, false));
    cerr << "DFS Start: " << startX << " " << startY << endl;
    dfs(startX, startY);
    leftToVisit.assign(height, vector<bool>(width, true));  //শুরুতে সবাই left to visit
    // cerr << "dfs done" << endl;
}

struct Game {
    int myScore, oppScore;
    int myFlagX, myFlagY, oppFlagX, oppFlagY;
    int myFlagCarrier, oppFlagCarrier;

    int myAliveMinionCnt;
    vector<Minion> myMinions;
    bitset<5> isAlive;

    int visibleMinionCnt;
    vector<Minion> oppMinions;
    bitset<5> isOppVisible;

    int visibleCoinCnt;
    vector<Coin> visibleCoins;

    vector<Operation> operations;

    Game()
    {
        myMinions.resize(5);
        oppMinions.resize(5);
    }

    // initial things
    inline void readFlag(bool my)
    {
        if (my) {
            cin >> myFlagX >> myFlagY;
            cin >> myFlagCarrier;
        }
        else {
            cin >> oppFlagX >> oppFlagY;
            cin >> oppFlagCarrier;
            if (oppFlagCarrier != -1 && oppFlagCarrier != mandatoryCarrier)
                if (oppFlagCarrier == flagDefender) flagDefender = mandatoryCarrier;
            mandatoryCarrier = oppFlagCarrier;  //না চাইতেই নিয়ে যখন
                                                //নিসে, ওকেই দায়িত্ব দাও
            if (explorerMinions.count(mandatoryCarrier)) explorerMinions.erase(mandatoryCarrier);
        }
        ign();
    }

    // game loop things
    inline void readScores()
    {
        cin >> myScore >> oppScore;
        ign();
    }

    void readMyMinions()
    {
        isAlive.reset();
        cin >> myAliveMinionCnt;
        ign();

        for (int i = 0; i < myAliveMinionCnt; i++) {
            int id;
            cin >> id;
            ign();
            isAlive[id] = true;
            myMinions[id].readMinion();
        }

        if (n_minions == -1) {
            n_minions = myAliveMinionCnt;
            for (int i = 0; i < n_minions; i++) explorerMinions.insert(i);
            // need a flag defender
            // choose the minion that is closest to my flag (my base)
            flagDefender = 0;
            int least_distance = distFromMyBase[myMinions[0].posX][myMinions[0].posY];
            // but 0 might be blocked somewhere
            while (distFromMyBase[myMinions[flagDefender].posX][myMinions[flagDefender].posY] >=
                   1000)
                flagDefender++;
            for (int i = 0; i < n_minions; i++) {
                int xx = myMinions[i].posX;
                int yy = myMinions[i].posY;
                if (distFromMyBase[xx][yy] < least_distance) {
                    least_distance = distFromMyBase[xx][yy];
                    flagDefender = i;
                }
            }
            explorerMinions.erase(flagDefender);

            // explorer দের কাজ distribute করে দেয়া লাগবে properly
            cellsToExplore.resize(n_minions);
            assignedExploration.assign(n_minions, -1);

            const int sz = not_visited.size() / (n_minions - 2);
            // cerr << "Size: " << sz << endl;
            for (int idx = 0; idx < n_minions - 2; idx++) {
                int start = sz * idx;
                int end = (idx + 1) * sz - 1;
                if (idx == n_minions - 3) end = not_visited.size() - 1;
                // cerr << idx << " " << start << " " << end << endl;
                for (int now = start; now <= end; now++) {
                    cellsToExplore[idx].push_back(not_visited[now]);
                }
            }

            // coin assignent initialization
            assignedCoins.resize(n_minions);
            for (int i = 0; i < assignedCoins.size(); i++) {
                assignedCoins[i].posX = -1;
                assignedCoins[i].posY = -1;
            }
        }

        if (mandatoryCarrier == -1 || !isAlive[mandatoryCarrier]) {
            // choose the minion that is the closest to opponent flag
            cerr << "mandatory carrier dead" << endl;
            distOptional = distFromOppBase;
            if (oppFlagBaseX != oppFlagX || oppFlagBaseY != oppFlagY)
                bfs({oppFlagX, oppFlagY}, distOptional);
            int least_distance = -1;
            for (int i = 0; i < n_minions; i++) {
                if (!isAlive[i]) continue;
                int xx = myMinions[i].posX;
                int yy = myMinions[i].posY;
                if (distOptional[xx][yy] >= 1000) continue;
                if (myAliveMinionCnt >= 2 && i == flagDefender) continue;
                if (least_distance == -1 || distOptional[xx][yy] < least_distance) {
                    mandatoryCarrier = i;
                    least_distance = distOptional[xx][yy];
                }
            }
            explorerMinions.erase(mandatoryCarrier);
        }

        if (!isAlive[flagDefender] && myAliveMinionCnt >= 2) {
            // if there are >= 2 minions, then one can be flag defender,
            // right? choose the minion that is the closest to my flag
            cerr << "flag defender dead" << endl;
            distOptional = distFromMyBase;
            if (myFlagBaseX != myFlagX || myFlagBaseY != myFlagY)
                bfs({myFlagX, myFlagY}, distOptional);
            int least_distance = -1;
            for (int i = 0; i < n_minions; i++) {
                if (!isAlive[i]) continue;
                int xx = myMinions[i].posX;
                int yy = myMinions[i].posY;
                if (i == mandatoryCarrier) continue;
                if (distOptional[xx][yy] >= 1000) continue;
                if (least_distance == -1 || distOptional[xx][yy] < least_distance) {
                    flagDefender = i;
                    least_distance = distOptional[xx][yy];
                }
            }
            explorerMinions.erase(mandatoryCarrier);
        }
        if (explorerMinions.count(mandatoryCarrier)) explorerMinions.erase(mandatoryCarrier);
        if (explorerMinions.count(flagDefender)) explorerMinions.erase(flagDefender);

        if (assignedExploration.back() == -1) {
            //কে কোনটা explore করবে এটা শুরুতে ঠিক করে দাও
            assignedExploration.back() = 0;
            int id = 0;
            for (int i = 0; i < n_minions; i++) {
                if (i == mandatoryCarrier || i == flagDefender) continue;
                // cerr << "assigned " << id << " to " << i << endl;
                assignedExploration[i] = id++;
            }
        }
    }

    void readOppMinions()
    {
        isOppVisible.reset();
        cin >> visibleMinionCnt;
        ign();
        for (int i = 0; i < visibleMinionCnt; i++) {
            int id;
            cin >> id;
            ign();
            isOppVisible[id] = true;
            oppMinions[id].readMinion();
        }
    }

    void readCoins()
    {
        cin >> visibleCoinCnt;
        ign();
        visibleCoins.resize(visibleCoinCnt);
        for (int i = 0; i < visibleCoinCnt; i++) visibleCoins[i].readCoin();
        // cerr << "Read Coins done" << endl;
    }

    void takeOperation(opTypes t, int id, bool& marker, int x = -1, int y = -1)
    {
        if (moveDone) return;
        int cost = 0;
        if (t == FIRE)
            cost = powerups[0].price;
        else if (t == FREEZE)
            cost = powerups[1].price;
        else if (t == MINE)
            cost = powerups[2].price;
        if (myScore < cost) return;
        operations.push_back(Operation(t, id, x, y));
        marker = true;
    }

    bool askSameLine(int x1, int y1, int x2, int y2, bool wantAdjAnalysis, int targetID,
                     const Game& last)
    {
        // cerr << "x1: " << x1 << " y1: " << y1 << " x2: " << x2 << " y2: " << y2 << " "
        //      << (wantAdjAnalysis ? "true" : "false") << " " << last.isOppVisible[targetID] <<
        //      endl;
        int lastX = -1, lastY = -1;
        if (wantAdjAnalysis && !firstTime && last.isOppVisible[targetID]) {
            lastX = last.oppMinions[targetID].posX;
            lastY = last.oppMinions[targetID].posY;
        }
        if (wantAdjAnalysis)
            return sameLine(x1, y1, x2, y2, true, lastX, lastY);
        else
            return sameLine(x1, y1, x2, y2, false, x2, y2);
    }

    void calculateVisibleMinions(int id, pair<int, int>& affected, pair<int, int>& death,
                                 pair<int, int>& alreadyFrozen, const Game& last, bool wantAnalysis)
    {
        // how many minions from both sides this minion of mine affects...
        affected = death = alreadyFrozen = {0, 0};
        int xx = myMinions[id].posX;
        int yy = myMinions[id].posY;
        // cerr << "ID: " << id << " position: " << xx << " " << yy << endl;
        for (int i = 0; i < n_minions; i++) {
            if (i == id || !isAlive[i]) continue;
            bool ret = askSameLine(xx, yy, myMinions[i].posX, myMinions[i].posY, false, i, last);
            if (ret) {
                // cerr << "My: " << id << " and " << i << " in same line" << endl;
                affected.first++;
                if (myMinions[i].health <= powerups[0].damage) death.first++;
                if (myMinions[i].timeout > 0) alreadyFrozen.first++;
            }
        }
        for (int i = 0; i < n_minions; i++) {
            if (!isOppVisible[i]) continue;
            bool ret =
                askSameLine(xx, yy, oppMinions[i].posX, oppMinions[i].posY, wantAnalysis, i, last);
            if (ret) {
                // cerr << "Opp: " << id << " and " << i << " in same line" << endl;
                affected.second++;
                if (oppMinions[i].health <= powerups[0].damage) death.second++;
                if (oppMinions[i].timeout > 0) alreadyFrozen.second++;
            }
        }
    }

    vector<int> getVisibleMinions(int id, const Game& last)
    {
        vector<int> v;
        int xx = myMinions[id].posX;
        int yy = myMinions[id].posY;
        for (int i = 0; i < n_minions; i++) {
            if (!isOppVisible[i]) continue;
            if (askSameLine(xx, yy, oppMinions[i].posX, oppMinions[i].posY, false, -1, last))
                v.push_back(i);
        }
        sort(v.begin(), v.end(), [&](const int a, const int b) {
            int d1 = abs(xx - oppMinions[a].posX) + abs(yy - oppMinions[a].posY);
            int d2 = abs(xx - oppMinions[b].posX) + abs(yy - oppMinions[b].posY);
            return d1 < d2;
        });
        return v;
    }

    void printOperations()
    {
        const int nn = operations.size();
        // cerr << "operations getting printed here" << endl;
        for (int i = 0; i < nn; i++) {
            operations[i].print();
            if (i == nn - 1)
                cout << endl;
            else
                cout << " | ";
        }
    }

    void doThings(const Game& last)
    {
        vector<int> v;
        // fixing the priorities
        v.push_back(mandatoryCarrier);
        if (flagDefender != mandatoryCarrier) v.push_back(flagDefender);
        for (int i = 0; i < n_minions; i++) {
            auto it = find(v.begin(), v.end(), i);
            if (it == v.end()) v.push_back(i);
        }
        for (int i = 0; i < v.size(); i++) cerr << v[i] << " ";
        cerr << endl;
        for (int j = 0; j < n_minions; j++) {
            int i = v[j];
            if (!isAlive[i]) continue;  // dead already
            int xx = myMinions[i].posX;
            int yy = myMinions[i].posY;
            pair<int, int> a1, d1, f1, a2, d2, f2;
            calculateVisibleMinions(i, a1, d1, f1, last, true);   // with adj analysis
            calculateVisibleMinions(i, a2, d2, f2, last, false);  // without adj analysis
            auto visibleOnes = getVisibleMinions(i, last);
            int maxOppHealth = 0, needed_fire_spell = 0;
            if (!visibleOnes.empty()) {
                maxOppHealth = oppMinions[visibleOnes.front()].health;
                for (int k = 1; k < visibleOnes.size(); k++)
                    maxOppHealth = max(maxOppHealth, oppMinions[visibleOnes[k]].health);
                needed_fire_spell = (maxOppHealth + powerups[0].damage - 1) / powerups[0].damage;
            }

            cerr << "ID: " << i << ", ";
            if (i == mandatoryCarrier)
                cerr << "mandatory carrier ";
            else if (i == flagDefender)
                cerr << "flag defender ";
            else
                cerr << "explorer ";
            cerr << xx << " " << yy << endl;
            // cerr << "Assigned Coin: " << assignedCoins[i].posX << " " << assignedCoins[i].posY
            //      << endl;

            // cerr << distFromMyBase[xx][yy] << " " <<
            // distFromOppBase[xx][yy]
            //      << endl;

            cerr << "Visible opponent minions: ";
            for (int i = 0; i < visibleOnes.size(); i++) {
                cerr << visibleOnes[i] << " ";
            }
            cerr << endl;

            cerr << "{" << a1.first << ", " << a1.second << "}, {" << d1.first << ", " << d1.second
                 << "}, {" << f1.first << ", " << f1.second << "}" << endl;
            cerr << "{" << a2.first << ", " << a2.second << "}, {" << d2.first << ", " << d2.second
                 << "}, {" << f2.first << ", " << f2.second << "}" << endl;
            // cerr << maxOppHealth << " " << needed_fire_spell << endl;
            moveDone = false;
            // moveDone check will be done inside the function

            int movX = -1, movY = -1;
            if (i == mandatoryCarrier) {
                if (oppFlagCarrier == -1)
                    movX = oppFlagX, movY = oppFlagY;
                else
                    movX = myFlagBaseX, movY = myFlagBaseY;
            }
            else if (i == flagDefender)
                movX = myFlagX, movY = myFlagY;

            if (a2.second > 0 && !firstTime) {
                int distNow = distFromMyBase[xx][yy];
                int score = INF;
                for (int dir = 0; dir < 4; dir++) {
                    int newX = dx[dir] + xx;
                    int newY = dy[dir] + yy;
                    if (!isValid(newX, newY)) continue;
                    if (askSameLine(newX, newY, last.myMinions[i].posX, last.myMinions[i].posY,
                                    false, -1, last))
                        continue;
                    if (i == oppFlagCarrier && distFromMyBase[newX][newY] >= distNow - 1 + 10)
                        continue;
                    else if (distFromMyBase[newX][newY] < score) {
                        score = distFromMyBase[newX][newY];
                        movX = newX;
                        movY = newY;
                    }
                }
            }

            if (i == mandatoryCarrier) {
                if (oppFlagCarrier == mandatoryCarrier ||
                    askSameLine(xx, yy, oppFlagX, oppFlagY, false, -1, last)) {
                    if (a2.second > f2.second && myScore >= powerups[1].price)
                        takeOperation(FREEZE, i, moveDone);
                    else
                        takeOperation(MOVE, i, moveDone, movX, movY);
                }

                if (!firstTime && myScore >= powerups[0].price && !visibleOnes.empty() &&
                    last.oppMinions[visibleOnes.front()].timeout == 1 &&
                    a1.first == 0) {  //এতোক্ষণ frozen ছিল
                    takeOperation(FIRE, i, moveDone);
                }
                if (oppFlagCarrier == mandatoryCarrier) {
                    // already has the opp flag with it
                    if (d1.first == 0 && d1.second > 0 && myScore >= powerups[0].price)
                        takeOperation(FIRE, i, moveDone);
                    if (f2.second > 0 && a1.first == 0 && myScore >= powerups[0].price &&
                        movX == myFlagBaseX &&
                        movY == myFlagBaseY)  //আগেই freeze করে রাখসি, এখন fire না করলে advantage
                                              //হারাবো, তবে যদি রাস্তা থাকে তাহলে fire করা লাগবে না
                        takeOperation(FIRE, i, moveDone);
                    if (distFromMyBase[xx][yy] <= 10 && myScore >= powerups[1].price &&
                        a2.second > 0)  // so close, so just freeze?
                        takeOperation(FREEZE, i, moveDone);
                    if (!firstTime && myMinions[i].health < last.myMinions[i].health &&
                        d1.first == 0 && a2.second > 0 && !visibleOnes.empty()) {  // retaliate
                        if (f1.second == 0 && myScore >= powerups[1].price && a2.second > 0 &&
                            myMinions[i].health < oppMinions[visibleOnes.front()].health)
                            takeOperation(FREEZE, i, moveDone);
                        else if (myScore >= powerups[0].price * needed_fire_spell)
                            takeOperation(FIRE, i, moveDone);
                        else
                            takeOperation(MOVE, i, moveDone, movX, movY);
                    }
                    if (a1.first > 0 || firstTime || a2.second == 0)
                        takeOperation(MOVE, i, moveDone, movX,
                                      movY);  // no casualty
                    if (myMinions[i].health <= 2 * powerups[0].damage &&
                        myScore >= powerups[0].price &&
                        a2.second > 0) {  // if you are down on health,
                                          // do not take any risk
                        if (myScore >= powerups[0].price * needed_fire_spell)
                            takeOperation(FIRE, i, moveDone);
                        else if (myScore >= powerups[1].price)
                            takeOperation(FREEZE, i, moveDone);
                        else
                            takeOperation(MOVE, i, moveDone, movX, movY);
                    }
                    if (myMinions[i].health == last.myMinions[i].health)  // pacifism
                        takeOperation(MOVE, i, moveDone, movX, movY);
                    // insert new moves here if found
                    // if nothing has been done yet
                    takeOperation(MOVE, i, moveDone, movX, movY);
                }
                else {
                    // so not got the flag yet
                    if (f2.second > 0 && a1.first == 0 &&
                        myScore >=
                            powerups[0]
                                .price)  //আগেই freeze করে রাখসি, এখন fire না করলে advantage হারাবো
                        takeOperation(FIRE, i, moveDone);
                    if (!firstTime && myMinions[i].health < last.myMinions[i].health &&
                        d1.first == 0 && a2.second > 0 && !visibleOnes.empty()) {  // retaliate
                        if (f1.second == 0 && myScore >= powerups[1].price && a2.second > 0 &&
                            myMinions[i].health < oppMinions[visibleOnes.front()].health)
                            takeOperation(FREEZE, i, moveDone);
                        else if (myScore >= powerups[0].price * needed_fire_spell)
                            takeOperation(FIRE, i, moveDone);
                        else
                            takeOperation(MOVE, i, moveDone, movX, movY);
                    }
                    bool ret = askSameLine(xx, yy, oppFlagX, oppFlagY, false, -1, last);
                    if (ret) {  // so reached the same line as flag
                        if (a2.second > 0) {
                            if (last.myMinions[i].health == myMinions[i].health) {
                                //আমাকে কিছু করতেসে না
                                takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                            }
                            //আমাকে কিছু করতেসে এখন!
                            if (d1.first == 0 &&
                                myScore >=
                                    needed_fire_spell * powerups[0].price)  // flag ছিনিয়ে আনো :3
                                takeOperation(FIRE, i, moveDone);
                            if (myScore >= powerups[1].price) {  //আমার minion আছে
                                //এই probability আসলে কম, but regardless
                                //বের করো flag কে পাহারা দিতেসে
                                //তারা already frozen কি না, থাকলে তো আবার
                                //দেয়া উচিত না
                                bool alreadyFrozen = true;
                                for (int i = 0; i < n_minions; i++) {
                                    if (!isOppVisible[i]) continue;
                                    if (!askSameLine(xx, yy, oppMinions[i].posX, oppMinions[i].posY,
                                                     false, -1,
                                                     last))  //এখন analysis দরকার নাই
                                        continue;
                                    if (oppMinions[i].timeout == 0) {
                                        alreadyFrozen = false;
                                        break;
                                    }
                                }
                                if (!alreadyFrozen &&
                                    myMinions[i].health <
                                        oppMinions[visibleOnes.front()].health)  // then freeze
                                    takeOperation(FREEZE, i, moveDone);
                                else  // then just proceed
                                    takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                            }
                        }
                        takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                    }
                    // line এ যাওনাই, তাহলে যাও এখন
                    takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                }
            }

            else if (i == flagDefender) {
                // আগে নিজে flag এর কাছে পৌঁছায় নাও
                // or no casualty
                if (!firstTime && myScore >= powerups[0].price && !visibleOnes.empty() &&
                    last.oppMinions[visibleOnes.front()].timeout == 1 &&
                    a1.first == 0)  //এতোক্ষণ frozen ছিল
                    takeOperation(FIRE, i, moveDone);
                if (!askSameLine(xx, yy, myFlagX, myFlagY, false, -1, last) || firstTime)
                    takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                if (a1.first > 0 &&
                    distFromOppBase[myFlagX][myFlagY] >=
                        30)  // casualty হবে, প্লাস অনেক দূরেও আছি, মারামারি করার কী দরকার
                    takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                if (f2.second > 0 && a1.first == 0 &&
                    myScore >=
                        powerups[0]
                            .price)  //আগেই freeze করে রাখসি, এখন fire না করলে advantage হারাবো
                    takeOperation(FIRE, i, moveDone);

                if (myFlagCarrier == -1) {
                    //এখন flag পড়ে আছে
                    // প্রথমে দেখো কেউ আসতেসে কিনা
                    if (d1.first == 0 && d1.second > 0 &&
                        myScore >= powerups[0].price)  //এখন মারলে নিশ্চিত মৃত্যু
                        takeOperation(FIRE, i, moveDone);
                    //চাইসিলাম আগে freeze করে then fire করতে, কিন্তু সেটা ভালো রেজাল্ট দিচ্ছে না
                    if (!firstTime && myMinions[i].health < last.myMinions[i].health &&
                        d1.first == 0 && a2.second > 0 && !visibleOnes.empty()) {  // retaliate
                        if (f1.second == 0 && myScore >= powerups[1].price && a2.second > 0 &&
                            myMinions[i].health < oppMinions[visibleOnes.front()].health)
                            takeOperation(FREEZE, i, moveDone);
                        else if (myScore >= powerups[0].price * needed_fire_spell)
                            takeOperation(FIRE, i, moveDone);
                        else
                            takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                    }
                    if (a2.second > 0 &&
                        myScore >= needed_fire_spell * powerups[0].price)  // someone coming
                        takeOperation(FIRE, i, moveDone);
                    // nobody coming or আসলেও মারার টাকা নাই
                    takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                }

                else {
                    // flag নিয়ে যাইতেসে
                    if (!firstTime && myMinions[i].health < last.myMinions[i].health &&
                        d1.first == 0 && a2.second > 0 && !visibleOnes.empty()) {  // retaliate
                        if (f1.second == 0 && myScore >= powerups[1].price && a2.second > 0 &&
                            myMinions[i].health < oppMinions[visibleOnes.front()].health)
                            takeOperation(FREEZE, i, moveDone);
                        else if (myScore >= powerups[0].price * needed_fire_spell)
                            takeOperation(FIRE, i, moveDone);
                        else
                            takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                    }
                    if (oppFlagCarrier != -1 &&
                        distFromMyBase[oppFlagX][oppFlagY] <=
                            distFromOppBase[myFlagX][myFlagY] - 10)  //কাছাকাছি চলে গেসে আমারটা
                        takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                    if (a1.second > 0 && myScore >= needed_fire_spell * powerups[0].price)
                        takeOperation(FIRE, i, moveDone);
                    else if (myScore >= powerups[1].price && a1.second > 0)
                        takeOperation(FREEZE, i, moveDone);
                    if (a1.first == 0 &&
                        askSameLine(xx, yy, myFlagX, myFlagY, true, myFlagCarrier, last) &&
                        myScore >= (((oppMinions[myFlagCarrier].health + powerups[0].damage - 1) /
                                     powerups[0].damage) *
                                    powerups[0].price))
                        takeOperation(FIRE, i, moveDone);
                    if (oppFlagCarrier != -1 &&
                        distFromMyBase[oppFlagX][oppFlagY] - distFromOppBase[myFlagX][myFlagY] <=
                            5 &&
                        a1.second > 0)
                        takeOperation(FREEZE, i, moveDone);
                    takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                }
                takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
            }

            else if (explorerMinions.count(i)) {
                // explorer দের মারামারি করার দরকার নাই, unless
                // myFlagCarrier কে পায়
                if (!firstTime && myScore >= powerups[0].price && !visibleOnes.empty() &&
                    last.oppMinions[visibleOnes.front()].timeout == 1 &&
                    a1.first == 0)  //এতোক্ষণ frozen ছিল
                    if (movX == -1)
                        takeOperation(FIRE, i, moveDone);
                    else
                        takeOperation(MOVE, i, moveDone, movX, movY);
                // retaliation is costly, no need to adopt it if you can evade
                if (!firstTime && myMinions[i].health < last.myMinions[i].health && d1.first == 0 &&
                    a2.second > 0 && !visibleOnes.empty()) {  // retaliate
                    if (movX != -1 && myAliveMinionCnt >= 2)
                        takeOperation(MOVE, i, moveDone, movX, movY);
                    if (f1.second == 0 && myScore >= powerups[1].price &&
                        myMinions[i].health < oppMinions[visibleOnes.front()].health)
                        takeOperation(FREEZE, i, moveDone);
                    else if (myScore >= powerups[0].price * needed_fire_spell)
                        takeOperation(FIRE, i, moveDone);
                }
                if (myScore >= powerups[0].price && a1.first == 0 && myFlagCarrier != -1) {
                    if (askSameLine(xx, yy, myFlagX, myFlagY, true, myFlagCarrier, last))
                        takeOperation(FIRE, i, moveDone);
                }
                if (f2.second > 0 && a1.first == 0 &&
                    myScore >=
                        powerups[0]
                            .price)  //আগেই freeze করে রাখসি, এখন fire না করলে advantage হারাবো
                    takeOperation(FIRE, i, moveDone);
                if (!moveDone) {
                    pair<int, int> dest = {-1, -1};
                    //আগে দেখো visible coin আছে নাকি?
                    if (assignedCoins[i].posX != -1) {  //একটা coin assign করা আছে
                        dest = {assignedCoins[i].posX, assignedCoins[i].posY};
                    }
                    else if (visibleCoinCnt > 0) {
                        int closestDist = -1, closestOne = -1;
                        for (int k = 0; k < visibleCoinCnt; k++) {
                            if (visibleCoins[k].taken ||
                                !askSameLine(xx, yy, visibleCoins[k].posX, visibleCoins[k].posY,
                                             false, -1, last))
                                continue;
                            for (int l = 0; l < n_minions; l++) {
                                if (assignedCoins[i].posX == -1) continue;
                                if (askSameLine(assignedCoins[l].posX, assignedCoins[l].posY,
                                                visibleCoins[k].posX, visibleCoins[k].posY, false,
                                                -1, last))
                                    continue;
                            }
                            if (closestOne == -1 ||
                                (abs(visibleCoins[k].posX - xx) + abs(visibleCoins[k].posY - yy) >
                                 closestDist)) {
                                closestOne = k;
                                closestDist =
                                    abs(visibleCoins[k].posX - xx) + abs(visibleCoins[k].posY - yy);
                            }
                        }
                        if (closestOne != -1) {
                            dest = {visibleCoins[closestOne].posX, visibleCoins[closestOne].posY};
                            visibleCoins[closestOne].taken = true;
                            assignedCoins[i].posX = dest.first;
                            assignedCoins[i].posY = dest.second;
                        }
                    }
                    if (dest == make_pair(-1, -1)) {
                        int assigned_exploration_idx =
                            assignedExploration[i];  //একে কোন সেট explore করতে বলা হইসে
                        // cerr << i << " " << cellsToExplore[assigned_exploration_idx].size() <<
                        // endl;
                        if (!cellsToExplore[assigned_exploration_idx].empty()) {
                            assert(!cellsToExplore[assigned_exploration_idx].empty());
                            while (!cellsToExplore[assigned_exploration_idx].empty()) {
                                pair<int, int> p = cellsToExplore[assigned_exploration_idx].back();
                                if (leftToVisit[p.first][p.second]) {
                                    dest = p;
                                    break;
                                }
                                else
                                    cellsToExplore[assigned_exploration_idx].pop_back();
                            }
                        }
                        if (cellsToExplore[assigned_exploration_idx].empty() &&
                            dest == make_pair(-1, -1)) {
                            int cnt = 0;
                            while (true) {
                                cnt++;
                                if (cnt == n_minions) break;
                                assigned_exploration_idx++;
                                if (assigned_exploration_idx == n_minions)
                                    assigned_exploration_idx = 0;
                                if (!cellsToExplore[assigned_exploration_idx].empty()) break;
                            }
                            if (cnt == n_minions) {  //সব empty হয়ে গেসে
                                cerr << "very less probability of this happening though" << endl;
                                if (myFlagCarrier != -1)
                                    dest = {myFlagX, myFlagY};
                                else
                                    dest = {myFlagBaseX, myFlagBaseY};
                            }
                        }
                    }
                    takeOperation(MOVE, i, moveDone, dest.first, dest.second);
                }
            }
        }
    }
};

Game last;

int main()
{
    readGrid();
    readBases();
    readPowerUps();

    bfs({myFlagBaseX, myFlagBaseY}, distFromMyBase);
    bfs({oppFlagBaseX, oppFlagBaseY}, distFromOppBase);

    // explorer minions will just explore for more and more coins
    init();

    while (true) {
        Game cur;
        cur.readScores();
        cur.readFlag(true);
        cur.readFlag(false);
        cur.readMyMinions();
        cur.readOppMinions();
        cur.readCoins();

        cur.doThings(last);

        cur.printOperations();
        firstTime = false;
        last = cur;
    }
}