// --- PERISAI KONFLIK WINDOWS vs RAYLIB ---
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER

#include <windows.h>
#include <winscard.h>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <cmath>
#include "include/raylib.h"

#pragma comment(lib, "winscard.lib")

using namespace std;

// --- STRUKTUR DATA ---
struct Pemain {
    string nama;
    int baki;
    bool dalam_penjara;
    int kad_bebas_penjara;
    int giliran_dalam_penjara; 
};

struct KadMisteri {
    string teks;
    int nilai; 
    int jenis_khas; 
};

struct GameState {
    map<string, Pemain> bank;
    vector<string> urutan;
    int giliran;
};

// --- PEMBOLEH UBAH GLOBAL ---
map<string, Pemain> bank_data;
vector<string> urutan_pemain; 
int giliran_semasa = 0;       

vector<GameState> sejarah_bank;      
vector<GameState> masa_depan_bank;   
const int MAKSIMUM_PEMAIN = 6;

vector<string> senarai_token = {"Kereta", "Topi", "Kapal", "Seterika", "Anjing", "Kasut"};

enum SkrinApl { SKRIN_MULA, DALAM_GAME };
SkrinApl skrin_semasa = SKRIN_MULA;

atomic<bool> nfc_berjalan(true);
string nfc_uid_semasa = "";
mutex nfc_mutex;

enum StatusGame { IDLE, TUNGGU_TAP, MENU_PENJARA, TUNJUK_HASIL };
StatusGame status_game = IDLE;

string mesej_sistem = "Sedia. Sila pilih tindakan di menu.";
int draf_amaun = 0;    
int dadu1 = 1, dadu2 = 1;
string tindakan_aktif = ""; 
string kad_teks_paparan = ""; 
string uid_penjara_aktif = "";

// --- DATABASE KAD ---
vector<KadMisteri> kad_peluang = {
    {"Maju ke MULA: Terima RM200", 200, 0},
    {"Mundur 3 petak", 0, 0},
    {"Pergi ke PENJARA segera!", 0, 1}, 
    {"Kad Keluar Percuma dari Penjara", 0, 2}, 
    {"Bayar Denda Trafik RM15", -15, 0},
    {"Bayar Cukai Jalan RM20", -20, 0},
    {"Bayar Yuran Sekolah RM150", -150, 0},
    {"Terima Dividen Saham RM50", 50, 0},
    {"Menang Peraduan RM100", 100, 0},
    {"Baiki Kereta RM50", -50, 0}
};

vector<KadMisteri> kad_nasib = {
    {"Maju ke MULA: Terima RM200", 200, 0},
    {"Terima Bonus RM100", 100, 0},
    {"Terima Wang Hadiah RM25", 25, 0},
    {"Duit Terpijak RM20", 20, 0},
    {"Jual Saham RM50", 50, 0},
    {"Terima RM10 dari setiap pemain", 0, 3}, 
    {"Warisan Harta RM100", 100, 0},
    {"Bayar Bil Hospital RM100", -100, 0},
    {"Bayar Cukai Pendapatan RM200", -200, 0},
    {"Pergi ke PENJARA segera!", 0, 1}, 
    {"Kad Keluar Percuma dari Penjara", 0, 2} 
};

// --- FUNGSI TIMELINE (UNDO & REDO) ---
void SimpanSejarah() {
    GameState state_kini = {bank_data, urutan_pemain, giliran_semasa};
    sejarah_bank.push_back(state_kini);
    if(sejarah_bank.size() > 10) sejarah_bank.erase(sejarah_bank.begin()); 
    masa_depan_bank.clear(); 
}

void LakukanUndo() {
    if (!sejarah_bank.empty()) {
        GameState state_kini = {bank_data, urutan_pemain, giliran_semasa};
        masa_depan_bank.push_back(state_kini);
        
        bank_data = sejarah_bank.back().bank;
        urutan_pemain = sejarah_bank.back().urutan;
        giliran_semasa = sejarah_bank.back().giliran;
        sejarah_bank.pop_back();
        mesej_sistem = "UNDO BERJAYA: Kembali 1 langkah.";
    }
}

void LakukanRedo() {
    if (!masa_depan_bank.empty()) {
        GameState state_kini = {bank_data, urutan_pemain, giliran_semasa};
        sejarah_bank.push_back(state_kini);
        
        bank_data = masa_depan_bank.back().bank;
        urutan_pemain = masa_depan_bank.back().urutan;
        giliran_semasa = masa_depan_bank.back().giliran;
        masa_depan_bank.pop_back();
        mesej_sistem = "REDO BERJAYA: Maju 1 langkah.";
    }
}

void Seterusnya() {
    if (urutan_pemain.size() > 0) {
        giliran_semasa = (giliran_semasa + 1) % urutan_pemain.size();
        mesej_sistem = "Giliran Seterusnya: " + bank_data[urutan_pemain[giliran_semasa]].nama;
    }
}

// --- FUNGSI BELAKANG TABIR (NFC THREAD) ---
void gelung_baca_nfc() {
    SCARDCONTEXT hContext;
    if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext) != SCARD_S_SUCCESS) return; 

    while (nfc_berjalan) {
        LPTSTR pmszReaders = NULL;
        DWORD cch = SCARD_AUTOALLOCATE;
        
        if (SCardListReaders(hContext, NULL, (LPTSTR)&pmszReaders, &cch) == SCARD_S_SUCCESS && pmszReaders != NULL) {
            SCARDHANDLE hCard;
            DWORD dwActiveProtocol;
            
            if (SCardConnect(hContext, pmszReaders, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol) == SCARD_S_SUCCESS) {
                BYTE pbSendBuffer[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
                BYTE pbRecvBuffer[258];
                DWORD cbRecvLength = sizeof(pbRecvBuffer);
                SCARD_IO_REQUEST pioSendPci;
                pioSendPci.dwProtocol = dwActiveProtocol;
                pioSendPci.cbPciLength = sizeof(pioSendPci);

                if (SCardTransmit(hCard, &pioSendPci, pbSendBuffer, sizeof(pbSendBuffer), NULL, pbRecvBuffer, &cbRecvLength) == SCARD_S_SUCCESS) {
                    if (cbRecvLength >= 2 && pbRecvBuffer[cbRecvLength - 2] == 0x90 && pbRecvBuffer[cbRecvLength - 1] == 0x00) {
                        stringstream ss;
                        for (DWORD i = 0; i < cbRecvLength - 2; i++) {
                            ss << hex << uppercase << setw(2) << setfill('0') << (int)pbRecvBuffer[i];
                        }
                        lock_guard<mutex> lock(nfc_mutex);
                        nfc_uid_semasa = ss.str();
                    }
                }
                SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            } else {
                lock_guard<mutex> lock(nfc_mutex);
                nfc_uid_semasa = "";
            }
            SCardFreeMemory(hContext, pmszReaders);
        } else {
            SCardReleaseContext(hContext);
            SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
            lock_guard<mutex> lock(nfc_mutex);
            nfc_uid_semasa = "";
        }
        Sleep(300);
    }
    SCardReleaseContext(hContext);
}

// --- FUNGSI LUKISAN GRAFIK ---
void LukisButang(Rectangle rect, string teks, Color warnaLatar, Color warnaTeks, Camera2D kamera) {
    Vector2 p = GetMousePosition();
    if (GetTouchPointCount() > 0) p = GetTouchPosition(0);
    Vector2 virtualP = GetScreenToWorld2D(p, kamera);
    
    DrawRectangleRec(rect, warnaLatar);
    if (CheckCollisionPointRec(virtualP, rect)) {
        DrawRectangleRec(rect, {255, 255, 255, 50}); 
    }
    
    DrawRectangleLinesEx(rect, 2, WHITE);
    int textWidth = MeasureText(teks.c_str(), 20);
    DrawText(teks.c_str(), rect.x + (rect.width / 2) - (textWidth / 2), rect.y + (rect.height / 2) - 10, 20, warnaTeks);
}

void LukisDadu(int x, int y, int nilai) {
    int saiz = 50; 
    DrawRectangle(x, y, saiz, saiz, WHITE); 
    DrawRectangleLinesEx({(float)x, (float)y, (float)saiz, (float)saiz}, 2, BLACK); 
    Color dot = BLACK;
    int r = 5, d = 12, c = saiz / 2; 

    if (nilai == 1 || nilai == 3 || nilai == 5) DrawCircle(x + c, y + c, r, dot); 
    if (nilai >= 2) { DrawCircle(x + d, y + d, r, dot); DrawCircle(x + saiz - d, y + saiz - d, r, dot); }
    if (nilai >= 4) { DrawCircle(x + saiz - d, y + d, r, dot); DrawCircle(x + d, y + saiz - d, r, dot); }
    if (nilai == 6) { DrawCircle(x + d, y + c, r, dot); DrawCircle(x + saiz - d, y + c, r, dot); }
}

// --- FUNGSI UTAMA ---
int main() {
    InitWindow(0, 0, "MONOPOLY DIGITAL BANKING"); 
    ToggleFullscreen(); 
    
    InitAudioDevice(); 
    Music muzikLatar = LoadMusicStream("1.mp3");
    PlayMusicStream(muzikLatar);

    SetTargetFPS(60);

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float scale = min((float)screenW / 900.0f, (float)screenH / 650.0f);
    Vector2 offset = { (screenW - (900 * scale)) / 2.0f, (screenH - (650 * scale)) / 2.0f };

    Camera2D kamera = { 0 };
    kamera.target = { 0.0f, 0.0f };
    kamera.offset = offset;
    kamera.rotation = 0.0f;
    kamera.zoom = scale;

    thread thread_nfc(gelung_baca_nfc);

    Color cLatar = { 20, 20, 25, 255 };
    Color cPanel = { 35, 35, 45, 255 };
    Color cHijau = { 50, 220, 100, 255 };
    Color cMerah = { 240, 80, 80, 255 };
    Color cBiru = { 60, 150, 240, 255 };
    Color cKuning = { 240, 200, 50, 255 };

    KadMisteri kad_semasa_aktif = {"", 0, 0};
    
    // PEMBOLEH UBAH UNTUK SENTUHAN TULEN
    bool sentuhan_sebelumnya = false;

    while (!WindowShouldClose()) {
        UpdateMusicStream(muzikLatar);

        // ==========================================
        //  PENGESAN SENTUHAN TULEN (ANTI DOUBLE-CLICK)
        // ==========================================
        bool ada_sentuhan_skrin = (GetTouchPointCount() > 0);
        bool tap_baru_bermula = (ada_sentuhan_skrin && !sentuhan_sebelumnya);
        sentuhan_sebelumnya = ada_sentuhan_skrin;

        auto ButangDitekan = [&](Rectangle rect) {
            // 1. Semak jika Mouse PC ditekan
            Vector2 mPos = GetScreenToWorld2D(GetMousePosition(), kamera);
            if (CheckCollisionPointRec(mPos, rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                return true;
            }

            // 2. Semak jika Skrin Sentuh ditekan (Bertindak SERTA-MERTA)
            if (tap_baru_bermula) {
                Vector2 tPos = GetScreenToWorld2D(GetTouchPosition(0), kamera);
                if (CheckCollisionPointRec(tPos, rect)) {
                    return true;
                }
            }
            return false;
        };

        // ==========================================
        //  SKRIN MULA (HOME)
        // ==========================================
        if (skrin_semasa == SKRIN_MULA) {
            BeginDrawing();
            ClearBackground(BLACK);
            BeginMode2D(kamera);
            
            DrawRectangle(0, 0, 900, 650, cLatar);
            DrawText("MONOPOLY", 270, 150, 60, cHijau);
            DrawText("DIGITAL BANKING SYSTEM", 220, 220, 35, WHITE);

            Rectangle btnMula = { 300, 400, 300, 80 };
            LukisButang(btnMula, "MASUK KE BANK (HIBRID)", cBiru, WHITE, kamera);

            if (ButangDitekan(btnMula)) skrin_semasa = DALAM_GAME;

            EndMode2D();
            EndDrawing();
            continue;
        }

        // ==========================================
        //  LOGIK GILIRAN & PENJARA
        // ==========================================
        string uid_giliran = "";
        bool giliran_orang_penjara = false;
        if (!urutan_pemain.empty()) {
            uid_giliran = urutan_pemain[giliran_semasa];
            if (bank_data[uid_giliran].dalam_penjara) giliran_orang_penjara = true;
        }

        string uid_nfc = "";
        { lock_guard<mutex> lock(nfc_mutex); uid_nfc = nfc_uid_semasa; }

        // ==========================================
        //  LOGIK TRANSAKSI & PENDAFTARAN
        // ==========================================
        Rectangle btnTolak  = { 440, 220, 110, 40 };
        Rectangle btnTambah = { 670, 220, 110, 40 };
        Rectangle btnSahTransaksi = { 550, 280, 140, 40 };
        
        if (status_game == TUNGGU_TAP && tindakan_aktif == "BAYAR_TERIMA") {
            if (ButangDitekan(btnTolak)) draf_amaun -= 50;
            if (ButangDitekan(btnTambah)) draf_amaun += 50;
        }

        // TANGKAP KLIK MANUAL PADA NAMA PEMAIN (Hanya masa daftar / Urus Penjara Manual)
        string uid_manual_ditekan = "";
        if (status_game == TUNGGU_TAP && tindakan_aktif != "") {
            int py = 240; int px = 420; 
            for (string uid_pemain : urutan_pemain) {
                Rectangle btnPemain = { (float)px, (float)py, 100, 40 };
                if (ButangDitekan(btnPemain)) uid_manual_ditekan = uid_pemain; 
                px += 110;
                if (px > 750) { px = 420; py += 50; }
            }
        }

        Rectangle btnDaftarManual = { 650, 180, 200, 40 };
        if (status_game == TUNGGU_TAP && tindakan_aktif == "DAFTAR") {
            if (uid_nfc != "" || ButangDitekan(btnDaftarManual)) {
                if (bank_data.size() < MAKSIMUM_PEMAIN) {
                    SimpanSejarah(); 
                    string uid_baru = (uid_nfc != "") ? uid_nfc : "MANUAL_" + to_string(bank_data.size() + 1);
                    
                    if (bank_data.count(uid_baru)) {
                        mesej_sistem = "RALAT: Kad ini sudah didaftarkan!";
                        sejarah_bank.pop_back();
                    } else {
                        string nama_baru = senarai_token[bank_data.size()];
                        bank_data[uid_baru] = {nama_baru, 1500, false, 0, 0};
                        urutan_pemain.push_back(uid_baru);
                        mesej_sistem = "BERJAYA: " + nama_baru + " didaftarkan";
                    }
                } else {
                    mesej_sistem = "RALAT: Maksimum 6 pemain dicapai.";
                }
                status_game = TUNJUK_HASIL; tindakan_aktif = ""; 
            }
        }

        // PENGESAHAN TRANSAKSI AUTO-TARGET (Atau NFC Pihak Ketiga)
        if (status_game == TUNGGU_TAP && tindakan_aktif != "DAFTAR") {
            
            string target_id = "";
            if (uid_nfc != "") target_id = uid_nfc;
            else if (uid_manual_ditekan != "") target_id = uid_manual_ditekan;
            else if (ButangDitekan(btnSahTransaksi) && uid_giliran != "") target_id = uid_giliran;

            if (target_id != "") {
                SimpanSejarah(); 

                if (bank_data.count(target_id)) {
                    if (tindakan_aktif == "BAYAR_TERIMA") {
                        bank_data[target_id].baki += draf_amaun;
                        mesej_sistem = "BERJAYA: Transaksi untuk " + bank_data[target_id].nama;
                        status_game = TUNJUK_HASIL; tindakan_aktif = "";
                    }
                    else if (tindakan_aktif == "KAD_MISTERI") {
                        bank_data[target_id].baki += kad_semasa_aktif.nilai; 
                        
                        if (kad_semasa_aktif.jenis_khas == 1) { 
                            bank_data[target_id].dalam_penjara = true;
                            bank_data[target_id].giliran_dalam_penjara = 0;
                            mesej_sistem = bank_data[target_id].nama + " DIMASUKKAN KE PENJARA!";
                        } 
                        else if (kad_semasa_aktif.jenis_khas == 2) { 
                            bank_data[target_id].kad_bebas_penjara += 1;
                            mesej_sistem = bank_data[target_id].nama + " DAPAT KAD BEBAS PENJARA!";
                        }
                        else if (kad_semasa_aktif.jenis_khas == 3) { 
                            int kutipan = 0;
                            for (auto& p : bank_data) {
                                if (p.first != target_id) { p.second.baki -= 10; kutipan += 10; }
                            }
                            bank_data[target_id].baki += kutipan;
                            mesej_sistem = bank_data[target_id].nama + " Kutip RM" + to_string(kutipan) + " dari semua!";
                        } else {
                            mesej_sistem = "BERJAYA: Kad diaplikasikan pada " + bank_data[target_id].nama;
                        }
                        status_game = TUNJUK_HASIL; tindakan_aktif = "";
                    }
                    else if (tindakan_aktif == "URUS_MASUK_PENJARA") {
                        if (!bank_data[target_id].dalam_penjara) {
                            bank_data[target_id].dalam_penjara = true;
                            bank_data[target_id].giliran_dalam_penjara = 0;
                            mesej_sistem = bank_data[target_id].nama + " telah ditangkap dan masuk penjara.";
                            status_game = TUNJUK_HASIL; tindakan_aktif = "";
                        } else {
                            uid_penjara_aktif = target_id;
                            sejarah_bank.pop_back(); 
                            status_game = MENU_PENJARA;
                        }
                    }
                } else {
                    mesej_sistem = "RALAT: Kad/Pemain tidak dikenali.";
                    sejarah_bank.pop_back(); 
                    status_game = TUNJUK_HASIL; tindakan_aktif = "";
                }
            }
        }

        // ==========================================
        //  LOGIK MENU PENJARA (MANUAL / AUTO)
        // ==========================================
        Rectangle btnBayarDenda = { 420, 180, 200, 40 };
        Rectangle btnGunaKad    = { 640, 180, 200, 40 };
        Rectangle btnBalingDaduPenjara = { 420, 240, 420, 40 };
        Rectangle btnBatal      = { 420, 300, 200, 40 };

        if (status_game == MENU_PENJARA || (status_game == IDLE && giliran_orang_penjara)) {
            
            string id_target = (status_game == MENU_PENJARA) ? uid_penjara_aktif : uid_giliran;
            Pemain& p_penjara = bank_data[id_target];

            if (ButangDitekan(btnBayarDenda)) {
                SimpanSejarah();
                p_penjara.baki -= 50; 
                p_penjara.dalam_penjara = false;
                p_penjara.giliran_dalam_penjara = 0;
                mesej_sistem = p_penjara.nama + " BEBAS! (Denda RM50 dibayar)";
                status_game = TUNJUK_HASIL; tindakan_aktif = "";
            }
            if (ButangDitekan(btnGunaKad) && p_penjara.kad_bebas_penjara > 0) {
                SimpanSejarah();
                p_penjara.kad_bebas_penjara -= 1;
                p_penjara.dalam_penjara = false;
                p_penjara.giliran_dalam_penjara = 0;
                mesej_sistem = p_penjara.nama + " BEBAS! (Guna Kad)";
                status_game = TUNJUK_HASIL; tindakan_aktif = "";
            }
            if (ButangDitekan(btnBalingDaduPenjara)) {
                SimpanSejarah();
                dadu1 = GetRandomValue(1, 6); dadu2 = GetRandomValue(1, 6);
                
                if (dadu1 == dadu2) { 
                    p_penjara.dalam_penjara = false;
                    p_penjara.giliran_dalam_penjara = 0;
                    mesej_sistem = p_penjara.nama + " BEBAS! Dapat Double " + to_string(dadu1);
                    status_game = TUNJUK_HASIL; 
                } else {
                    p_penjara.giliran_dalam_penjara += 1; 
                    if (p_penjara.giliran_dalam_penjara >= 3) { 
                        p_penjara.baki -= 50; 
                        p_penjara.dalam_penjara = false;
                        p_penjara.giliran_dalam_penjara = 0;
                        mesej_sistem = "Gagal 3 Kali! " + p_penjara.nama + " didenda RM50 dan dibebaskan.";
                        status_game = TUNJUK_HASIL; 
                    } else {
                        mesej_sistem = p_penjara.nama + " Gagal Double. Giliran ditamatkan automatik.";
                        Seterusnya(); // Auto Skip
                        status_game = TUNJUK_HASIL; 
                    }
                }
                tindakan_aktif = "";
            }
            
            if (status_game == MENU_PENJARA && ButangDitekan(btnBatal)) {
                status_game = IDLE; tindakan_aktif = "";
                mesej_sistem = "Sedia. Sila pilih tindakan.";
            }
        }

        Rectangle btnOK = { 575, 200, 100, 40 };
        if (status_game == TUNJUK_HASIL) {
            if (ButangDitekan(btnOK)) {
                status_game = IDLE;
                mesej_sistem = "Sedia. Sila pilih tindakan.";
            }
        }

        // ==========================================
        //         LUKISAN UI DALAM GAME
        // ==========================================
        BeginDrawing();
        ClearBackground(BLACK); 
        BeginMode2D(kamera); 

        DrawRectangle(0, 0, 900, 650, cLatar);

        // HEADER
        DrawRectangle(0, 0, 900, 70, cPanel);
        DrawText("MONOPOLY DIGITAL BANKING", 20, 20, 30, cHijau);
        
        string statusNFC = (uid_nfc != "") ? "NFC: KAD DIKESAN" : "HIBRID MODE (NFC/Sentuh)";
        DrawText(statusNFC.c_str(), 580, 25, 20, uid_nfc != "" ? cHijau : cKuning);

        // PANEL KIRI
        DrawRectangle(20, 90, 360, 530, cPanel);
        DrawText("SENARAI PEMAIN", 100, 100, 20, WHITE);
        
        if (uid_giliran != "") {
            string nama_giliran = bank_data[uid_giliran].nama;
            DrawRectangle(30, 130, 340, 35, {60, 150, 240, 100}); 
            DrawText(("GILIRAN: " + nama_giliran).c_str(), 40, 138, 20, WHITE);
        }
        DrawLine(30, 175, 360, 175, GRAY);

        int y_pos = 185; 
        for (int i=0; i < urutan_pemain.size(); i++) {
            Pemain p = bank_data[urutan_pemain[i]];
            Color wNama = (i == giliran_semasa) ? WHITE : YELLOW;
            
            DrawText(TextFormat("%d. %s", i+1, p.nama.c_str()), 40, y_pos, 20, wNama);
            DrawText(TextFormat("RM %d", p.baki), 250, y_pos, 20, cHijau);
            
            if (p.dalam_penjara) {
                DrawText(TextFormat("[ DALAM PENJARA: %d/3 ]", p.giliran_dalam_penjara), 60, y_pos + 20, 15, cMerah);
            }
            if (p.kad_bebas_penjara > 0) {
                DrawText(TextFormat("[ KAD BEBAS: %d ]", p.kad_bebas_penjara), 230, y_pos + 20, 15, cKuning);
            }
            y_pos += 50;
        }

        // PANEL KANAN
        DrawRectangle(390, 90, 490, 210, cPanel); 
        
        if (status_game == IDLE) {
            if (giliran_orang_penjara) {
                Pemain p = bank_data[uid_giliran];
                DrawText(TextFormat("%s DALAM PENJARA! (Gagal: %d/3)", p.nama.c_str(), p.giliran_dalam_penjara), 420, 110, 18, cMerah);
                LukisButang(btnBayarDenda, "Bayar Denda RM50", cMerah, WHITE, kamera);
                if (p.kad_bebas_penjara > 0) LukisButang(btnGunaKad, "Guna Kad Bebas", cHijau, BLACK, kamera);
                else LukisButang(btnGunaKad, "Tiada Kad Bebas", GRAY, LIGHTGRAY, kamera); 
                LukisButang(btnBalingDaduPenjara, "BALING DADU (Cari Double)", cBiru, WHITE, kamera);
            } else {
                DrawText(mesej_sistem.c_str(), 420, 170, 20, LIGHTGRAY);
            }
        } 
        else if (status_game == MENU_PENJARA) {
            Pemain p = bank_data[uid_penjara_aktif];
            DrawText(TextFormat("URUS PENJARA: %s (Gagal: %d/3)", p.nama.c_str(), p.giliran_dalam_penjara), 420, 110, 20, ORANGE);
            LukisButang(btnBayarDenda, "Bayar Denda RM50", cMerah, WHITE, kamera);
            if (p.kad_bebas_penjara > 0) LukisButang(btnGunaKad, "Guna Kad Bebas", cHijau, BLACK, kamera);
            else LukisButang(btnGunaKad, "Tiada Kad Bebas", GRAY, LIGHTGRAY, kamera); 
            LukisButang(btnBalingDaduPenjara, "BALING DADU (Cari Double)", cBiru, WHITE, kamera);
            LukisButang(btnBatal, "BATAL", GRAY, WHITE, kamera);
        }
        else if (status_game == TUNGGU_TAP) {
            
            if (tindakan_aktif == "DAFTAR") {
                DrawText(">>> DAFTAR PEMAIN BARU <<<", 450, 100, 20, cKuning);
                DrawText("Sila Tap Kad NFC atau...", 420, 140, 20, WHITE);
                LukisButang(btnDaftarManual, "DAFTAR MANUAL (Tanpa Kad)", cBiru, WHITE, kamera);
            }
            else {
                string target_nama = uid_giliran != "" ? bank_data[uid_giliran].nama : "Sila pilih pemain";
                DrawText(TextFormat(">>> TRANSAKSI: %s <<<", target_nama.c_str()), 450, 100, 20, cKuning);

                if (tindakan_aktif == "BAYAR_TERIMA") {
                    DrawText(TextFormat("Jumlah: RM %d", draf_amaun), 480, 150, 25, (draf_amaun >= 0 ? cHijau : cMerah));
                    LukisButang(btnTolak, "- RM 50", cMerah, WHITE, kamera);
                    LukisButang(btnTambah, "+ RM 50", cHijau, BLACK, kamera);
                    LukisButang(btnSahTransaksi, "SAHKAN", cBiru, WHITE, kamera);
                } 
                else if (tindakan_aktif == "KAD_MISTERI") {
                    DrawText(kad_teks_paparan.c_str(), 420, 150, 18, WHITE);
                    LukisButang(btnSahTransaksi, "TERIMA", cBiru, WHITE, kamera);
                }
                else if (tindakan_aktif == "URUS_MASUK_PENJARA") {
                    DrawText("Sila tekan nama pemain yang akan disumbat", 410, 140, 18, cMerah);
                }

                // BUTANG NAMA UNTUK MANUAL OVERRIDE (Kalau nk byr org lain)
                if (tindakan_aktif != "DAFTAR") {
                    int py = 240; int px = 420;
                    for (string uid_pemain : urutan_pemain) {
                        Pemain p = bank_data[uid_pemain];
                        Rectangle btnPemain = { (float)px, (float)py, 100, 40 };
                        Color wBtn = p.dalam_penjara ? cMerah : Color{ 60, 60, 80, 255 };
                        LukisButang(btnPemain, p.nama, wBtn, WHITE, kamera);
                        px += 110;
                        if (px > 750) { px = 420; py += 50; }
                    }
                }
            }
        }
        else if (status_game == TUNJUK_HASIL) {
            DrawText(mesej_sistem.c_str(), 420, 150, 20, WHITE);
            LukisButang(btnOK, "OK", cBiru, WHITE, kamera);
        }

        // --- MENU BUTANG UTAMA ---
        Rectangle btnDaftar = { 390, 310, 230, 50 };
        Rectangle btnTerima = { 390, 370, 230, 50 };
        Rectangle btnBayar  = { 390, 430, 230, 50 };
        Rectangle btnPenjara= { 390, 490, 230, 50 }; 

        Rectangle btnPeluang = { 650, 310, 230, 50 };
        Rectangle btnNasib   = { 650, 370, 230, 50 };
        Rectangle btnDadu    = { 650, 430, 230, 50 };
        Rectangle btnGiliran = { 650, 490, 230, 50 }; 

        Rectangle btnUndo    = { 20, 600, 150, 35 };
        Rectangle btnRedo    = { 180, 600, 150, 35 }; 

        LukisButang(btnDaftar, "Daftar Pemain", cBiru, WHITE, kamera);
        LukisButang(btnTerima, "Terima Duit", cHijau, BLACK, kamera);
        LukisButang(btnBayar, "Bayar Bank", cMerah, WHITE, kamera);
        LukisButang(btnPenjara, "Masuk/Keluar Penjara", ORANGE, BLACK, kamera); 

        LukisButang(btnPeluang, "Kad Peluang", cKuning, BLACK, kamera);
        LukisButang(btnNasib, "Kad Nasib", PURPLE, WHITE, kamera);
        LukisButang(btnDadu, "Baling Dadu", LIGHTGRAY, BLACK, kamera);
        LukisButang(btnGiliran, "TAMAT GILIRAN >>", { 100, 200, 255, 255 }, BLACK, kamera); 

        LukisButang(btnUndo, "UNDO (Batal)", GRAY, WHITE, kamera); 
        LukisButang(btnRedo, "REDO (Ulang)", GRAY, WHITE, kamera); 

        // LUKIS DADU
        DrawText("Hasil Dadu:", 650, 570, 20, WHITE);
        LukisDadu(770, 555, dadu1); 
        LukisDadu(830, 555, dadu2); 

        // LOGIK KLIK BUTANG UTAMA
        if (status_game == IDLE && !giliran_orang_penjara) {
            if (ButangDitekan(btnUndo)) LakukanUndo();
            else if (ButangDitekan(btnRedo)) LakukanRedo();
            else if (ButangDitekan(btnGiliran)) {
                SimpanSejarah(); Seterusnya();
            }
            else if (ButangDitekan(btnDaftar)) {
                tindakan_aktif = "DAFTAR"; status_game = TUNGGU_TAP;
            }
            else if (ButangDitekan(btnDadu)) {
                dadu1 = GetRandomValue(1, 6); dadu2 = GetRandomValue(1, 6);
            }
            else if (uid_giliran != "") {
                if (ButangDitekan(btnTerima)) {
                    tindakan_aktif = "BAYAR_TERIMA"; draf_amaun = 100; status_game = TUNGGU_TAP;
                }
                else if (ButangDitekan(btnBayar)) {
                    tindakan_aktif = "BAYAR_TERIMA"; draf_amaun = -100; status_game = TUNGGU_TAP;
                }
                else if (ButangDitekan(btnPenjara)) {
                    tindakan_aktif = "URUS_MASUK_PENJARA"; status_game = TUNGGU_TAP;
                }
                else if (ButangDitekan(btnPeluang)) {
                    tindakan_aktif = "KAD_MISTERI";
                    int rIdx = GetRandomValue(0, kad_peluang.size() - 1);
                    kad_semasa_aktif = kad_peluang[rIdx];
                    kad_teks_paparan = "PELUANG: " + kad_semasa_aktif.teks;
                    status_game = TUNGGU_TAP;
                }
                else if (ButangDitekan(btnNasib)) {
                    tindakan_aktif = "KAD_MISTERI";
                    int rIdx = GetRandomValue(0, kad_nasib.size() - 1);
                    kad_semasa_aktif = kad_nasib[rIdx];
                    kad_teks_paparan = "NASIB: " + kad_semasa_aktif.teks;
                    status_game = TUNGGU_TAP;
                }
            }
        }
        else if (status_game == IDLE && giliran_orang_penjara) {
            if (ButangDitekan(btnUndo)) LakukanUndo();
            if (ButangDitekan(btnRedo)) LakukanRedo();
        }

        EndMode2D(); 
        EndDrawing();
    }

    nfc_berjalan = false; 
    thread_nfc.join(); 
    
    UnloadMusicStream(muzikLatar);
    CloseAudioDevice();

    CloseWindow();
    return 0;
}