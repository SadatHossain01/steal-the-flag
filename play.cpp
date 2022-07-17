#pragma GCC optimize("O3,inline")
#pragma GCC target("bmi,bmi2,lzcnt,popcnt")

#include <algorithm>
#include <bitset>
#include <deque>
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
deque<pair<int, int>> not_visited;
set<int> explorerMinions;
int myFlagBaseX, myFlagBaseY, oppFlagBaseX, oppFlagBaseY;
// this is for different calls of bfs()
vector<vector<bool>> visited;
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
    inline void readPowerUp() {
        cin >> name >> price >> damage;
        ign();
    }
};

Powerup powerups[3];  // 0 fire, 1 freeze, 2 mine

struct Minion {
    // int id; this is basically the array index
    int posX, posY, health, timeout;
    // read the id first, and then call minion[id].readMinion()
    inline void readMinion() {
        cin >> posX >> posY >> health >> timeout;
        ign();
        leftToVisit[posX][posY] = false;
    }
};

struct Coin {
    int posX, posY;
    inline void readCoin() {
        cin >> posX >> posY;
        ign();
    }
};

struct Operation {
    opTypes ty;
    int id, x, y;
    Operation(opTypes o, int id, int x = -1, int y = -1) : ty(o), id(id), x(x), y(y) {}
    void print() {
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

void readGrid() {
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

void readBases() {
    cin >> myFlagBaseX >> myFlagBaseY;
    ign();
    cin >> oppFlagBaseX >> oppFlagBaseY;
    ign();
}

void readPowerUps() {
    for (int i = 0; i < 3; i++) powerups[i].readPowerUp();
}

inline bool isValid(int x, int y) {
    return x >= 0 && x < height && y >= 0 && y < width && grid[x][y] == '.';
}

inline bool sameLine(int x1, int y1, int x2, int y2, bool wantAdjAnalysis, int lastX, int lastY) {
    // cerr << "x1: " << x1 << " y1: " << y1 << " x2: " << x2 << " y2: " << y2
    //      << " " << lastX << " " << lastY << endl;
    //  x2 y2 is the target
    bool ret = false;
    int now = -1;
    // first ensure সে এখানে বসে নাই ইচ্ছা করে, কারণ তা হলে যাওয়ার কয়টা জায়গা আছে
    // matter করে না, বসেই থাকবে যেহেতু
    if (wantAdjAnalysis && (lastX == -1 || (x2 != lastX || y2 != lastY))) {
        // lastX = -1 হলে last এর data নাই
        // so do the direct comparison
        // so dealing with an opponent
        //আমি তাকে যে লাইন ধরে attack করবো, তার যদি ওই লাইন ছেড়ে যাওয়ার ability
        //থাকে, তাহলে ওই attack করে লাভ নাই
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
    } else if (y1 == y2) {  // vertical matching
        now = prefixSumCol[x2][y2];
        if (x1 != 0) now -= prefixSumCol[x1 - 1][y1];
        if (now == 0) ret = true;
    }
    // cerr << now << " " << (ret ? "true" : "false") << endl;
    return ret;
}

void bfs(pair<int, int> s, vector<vector<int>>& dist) {
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

void dfs(int x, int y) {
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

void init() {
    // explorer minions will just explore for more and more coins
    leftToVisit.assign(height, vector<bool>(width,
                                            true));  // everything is left to visit at the start
    int startX = myFlagBaseX, startY = myFlagBaseY;
    int highestDistance = distFromOppBase[startX][startY];
    // starting cell should better be not around both bases, as those coins will
    // mostly be explored anyway
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (!visited[i][j]) continue;
            if (distFromMyBase[i][j] + distFromOppBase[i][j] +
                    min(distFromMyBase[i][j], distFromOppBase[i][j]) * 2 >
                highestDistance) {
                highestDistance = distFromMyBase[i][j] + distFromOppBase[i][j] +
                                  min(distFromMyBase[i][j], distFromOppBase[i][j]) * 2;
                startX = i;
                startY = j;
            }
        }
    }
    visited.assign(height, vector<bool>(width, false));
    cerr << "DFS Start: " << startX << " " << startY << endl;
    dfs(startX, startY);
    // cerr << "dfs done" << endl;
    // so the deque now contains all the unvisited cells in some path order
    // note that the deque might contain cells at front and back that are
    // already visited, so check for that in left_to_visit boolean array
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

    Game() {
        myMinions.resize(5);
        oppMinions.resize(5);
    }

    // initial things
    inline void readFlag(bool my) {
        if (my) {
            cin >> myFlagX >> myFlagY;
            cin >> myFlagCarrier;
        } else {
            cin >> oppFlagX >> oppFlagY;
            cin >> oppFlagCarrier;
            if (oppFlagCarrier != -1 && oppFlagCarrier != mandatoryCarrier)
                mandatoryCarrier = oppFlagCarrier;  //না চাইতেই নিয়ে যখন
                                                    //নিসে, ওকেই দায়িত্ব দাও
        }
        ign();
    }

    // game loop things
    inline void readScores() {
        cin >> myScore >> oppScore;
        ign();
    }

    void readMyMinions() {
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
        }

        if (mandatoryCarrier == -1 || !isAlive[mandatoryCarrier]) {
            // choose the minion that is the closest to opponent flag
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
    }

    void readOppMinions() {
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

    void readCoins() {
        cin >> visibleCoinCnt;
        ign();
        visibleCoins.resize(visibleCoinCnt);
        for (int i = 0; i < visibleCoinCnt; i++) visibleCoins[i].readCoin();
        // cerr << "Read Coins done" << endl;
    }

    void takeOperation(opTypes t, int id, bool& marker, int x = -1, int y = -1) {
        operations.push_back(Operation(t, id, x, y));
        marker = true;
    }

    bool askSameLine(int x1, int y1, int x2, int y2, bool wantAdjAnalysis, int targetID,
                     const Game& last) {
        // cerr << "x1: " << x1 << " y1: " << y1 << " x2: " << x2 << " y2: " <<
        // y2
        //      << " " << (wantAdjAnalysis ? "true" : "false") << " "
        //      << last.isOppVisible[targetID] << endl;
        int lastX = -1, lastY = -1;
        if (wantAdjAnalysis && !firstTime && !last.isOppVisible[targetID]) {
            lastX = last.oppMinions[targetID].posX;
            lastY = last.oppMinions[targetID].posY;
        }
        if (wantAdjAnalysis)
            return sameLine(x1, y1, x2, y2, true, lastX, lastY);
        else
            return sameLine(x1, y1, x2, y2, false, lastX, lastY);
    }

    void calculateVisibleMinions(int id, pair<int, int>& affected, pair<int, int>& death,
                                 pair<int, int>& alreadyFrozen, const Game& last) {
        // how many minions from both sides this minion of mine affects...
        affected = death = alreadyFrozen = {0, 0};
        int xx = myMinions[id].posX;
        int yy = myMinions[id].posY;
        // cerr << "ID: " << id << " position: " << xx << " " << yy << endl;
        for (int i = 0; i < n_minions; i++) {
            if (i == id || !isAlive[i]) continue;
            bool ret = askSameLine(xx, yy, myMinions[i].posX, myMinions[i].posY, false, i, last);
            if (ret) {
                // cerr << "My: " << id << " and " << i << " in same line"
                // << endl;
                affected.first++;
                if (myMinions[i].health <= powerups[0].damage) death.first++;
                if (myMinions[i].timeout > 0) alreadyFrozen.first++;
            }
        }
        for (int i = 0; i < n_minions; i++) {
            if (!isOppVisible[i]) continue;
            bool ret = askSameLine(xx, yy, oppMinions[i].posX, oppMinions[i].posY, true, i, last);
            if (ret) {
                // cerr << "Opp: " << id << " and " << i <<
                // " in same line"
                //      << endl;
                affected.second++;
                if (oppMinions[i].health <= powerups[0].damage) death.second++;
                if (oppMinions[i].timeout > 0) alreadyFrozen.second++;
            }
        }
    }

    void printOperations() {
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

    void doThings(const Game& last) {
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
            pair<int, int> a, d, f;
            calculateVisibleMinions(i, a, d, f, last);

            cerr << "ID: " << i << ", ";
            if (i == mandatoryCarrier)
                cerr << "mandatory carrier" << endl;
            else if (i == flagDefender)
                cerr << "flag defender" << endl;
            else
                cerr << "explorer" << endl;

            // cerr << distFromMyBase[xx][yy] << " " <<
            // distFromOppBase[xx][yy]
            //      << endl;

            cerr << "{" << a.first << ", " << a.second << "}, {" << d.first << ", " << d.second
                 << "}, {" << f.first << ", " << f.second << "}" << endl;
            moveDone = false;

            if (i == mandatoryCarrier) {
                if (oppFlagCarrier == mandatoryCarrier) {
                    // already has the opp flag with it
                    if (!moveDone && (d.first == 0 && d.second > 0 &&
                                      myScore >= powerups[0].price && distFromMyBase[xx][yy] <= 15))
                        takeOperation(FIRE, i, moveDone);
                    if (!moveDone &&
                        (distFromMyBase[xx][yy] <= 10 && myScore >= powerups[1].price &&
                         a.second > 0))  // so close, so just freeze?
                        takeOperation(FREEZE, i, moveDone);
                    if (!moveDone && (a.first > 0 || firstTime || a.second == 0))
                        takeOperation(MOVE, i, moveDone, myFlagBaseX,
                                      myFlagBaseY);  // no casualty
                    if (!moveDone && (myMinions[i].health <= 2 * powerups[0].damage &&
                                      myScore >= powerups[0].price &&
                                      a.second > 0))  // if you are down on health,
                                                      // do not take any risk
                        takeOperation(FIRE, i, moveDone);
                    if (!moveDone && (myMinions[i].health < last.myMinions[i].health &&
                                      a.second > 0 && myScore >= powerups[0].price))  // retaliate
                        takeOperation(FIRE, i, moveDone);
                    if (!moveDone && (myMinions[i].health == last.myMinions[i].health))  // pacifism
                        takeOperation(MOVE, i, moveDone, myFlagBaseX, myFlagBaseY);
                    // insert new moves here if found
                    if (!moveDone)  // if nothing has been done yet
                        takeOperation(MOVE, i, moveDone, myFlagBaseX, myFlagBaseY);
                } else {
                    // so not got the flag yet
                    bool ret = askSameLine(xx, yy, oppFlagX, oppFlagY, false, -1, last);
                    if (ret) {  // so reached the same line as flag
                        if (a.second > 0) {
                            if (!moveDone && last.myMinions[i].health == myMinions[i].health) {
                                //আমাকে কিছু করতেসে না
                                takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                            }
                            //আমাকে কিছু করতেসে এখন!
                            if (!moveDone && a.first == 0)  // flag ছিনিয়ে আনো :3
                                takeOperation(FIRE, i, moveDone);
                            if (!moveDone && a.first > 0) {  //আমার minion আছে
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
                                if (!alreadyFrozen)  // then freeze
                                    takeOperation(FREEZE, i, moveDone);
                                else  // then just proceed
                                    takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                            }
                        }
                        if (!moveDone) takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                    }
                    if (!moveDone)  // line এ যাওনাই, তাহলে যাও এখন
                        takeOperation(MOVE, i, moveDone, oppFlagX, oppFlagY);
                }
            }

            else if (i == flagDefender) {
                // আগে নিজে flag এর কাছে পৌঁছায় নাও
                // or no casualty
                if (!askSameLine(xx, yy, myFlagX, myFlagY, false, -1, last) || firstTime)
                    takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                if (!moveDone && a.first > 0 &&
                    distFromOppBase[myFlagX][myFlagY] >=
                        30)  // casualty হবে, প্লাস অনেক দূরেও আছি, মারামারি করার কী দরকার
                    takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);

                if (myFlagCarrier == -1) {
                    //এখন flag পড়ে আছে
                    // প্রথমে দেখো কেউ আসতেসে কিনা
                    if (!moveDone && d.first == 0 && d.second > 0 &&
                        myScore >= powerups[0].price)  //এখন মারলে নিশ্চিত মৃত্যু
                        takeOperation(FIRE, i, moveDone);
                    //চাইসিলাম আগে freeze করে then fire করতে, কিন্তু সেটা ভালো রেজাল্ট দিচ্ছে না
                    if (!moveDone &&
                        (a.second > 0 && myScore >= powerups[0].price))  // someone coming
                        takeOperation(FIRE, i, moveDone);
                    if (!moveDone)  // nobody coming or আসলেও মারার টাকা নাই
                        takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                }

                else {
                    // flag নিয়ে যাইতেসে
                    if (myScore >= powerups[0].price) {
                        if (!moveDone &&
                            askSameLine(xx, yy, myFlagX, myFlagY, true, myFlagCarrier, last)) {
                            //এই minion-ই তো নিয়ে যাইতেসে
                            // again, freeze integrate করার আইডিয়া ভালো কাজ করতেসিলো না
                            if (d.first == 0)
                                takeOperation(FIRE, i, moveDone);
                            else
                                takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                        }
                        if (!moveDone) takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                    }
                    if (!moveDone)  //টাকা নাই
                        takeOperation(MOVE, i, moveDone, myFlagX, myFlagY);
                }
            }

            else if (explorerMinions.count(i)) {
                //৫ টা থাকলে দুইটা পুরা একসাথে চলে, এটা ঠিক করতে হবে
                // this proper coin distribution is important, implement this
                // soon

                // explorer দের মারামারি করার দরকার নাই, unless
                // myFlagCarrier কে পায়
                if (myScore >= powerups[0].price && a.second > 0 && myFlagCarrier != -1) {
                    if (askSameLine(xx, yy, myFlagX, myFlagY, true, myFlagCarrier, last))
                        takeOperation(FIRE, i, moveDone);
                }
                if (!moveDone) {
                    pair<int, int> dest = {-1, -1};
                    if (!not_visited.empty()) {
                        if (*(explorerMinions.begin()) == i) {
                            auto now = not_visited.front();
                            while (!leftToVisit[now.first][now.second]) {
                                not_visited.pop_front();
                                if (!not_visited.empty())
                                    now = not_visited.front();
                                else
                                    break;
                            }
                            if (!not_visited.empty()) dest = now;
                        } else {
                            auto now = not_visited.back();
                            while (!leftToVisit[now.first][now.second]) {
                                not_visited.pop_back();
                                if (!not_visited.empty())
                                    now = not_visited.back();
                                else
                                    break;
                            }
                            if (!not_visited.empty()) dest = now;
                        }
                    }
                    if (dest == make_pair(-1, -1)) {
                        cerr << "this should almost never happen though" << endl;
                        if (myFlagCarrier != -1)
                            dest = {myFlagX, myFlagY};
                        else
                            dest = {myFlagBaseX, myFlagBaseY};
                    }
                    takeOperation(MOVE, i, moveDone, dest.first, dest.second);
                }
            }
        }
    }
};

Game last;

int main() {
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