🎩 Monopoly Digital Banking System (NFC + Touch)
Sistem Perbankan Digital Monopoly yang moden dan responsif, dibina khas untuk tablet Windows. Projek ini menggantikan wang kertas tradisional dengan terminal digital yang menyokong teknologi NFC (Near Field Communication) dan input skrin sentuh.

🚀 Ciri-Ciri Utama
Hibrid Input: Menyokong pengimbas kad NFC (Sony RC-S632/U) dan input manual skrin sentuh secara serentak.

Sistem Giliran Automatik: Menjejaki giliran pemain secara real-time dengan penunjuk visual yang jelas.

Mekanik Penjara Rasmi: Logik penjara mengikut undang-undang Monopoly (Baling dadu double, denda RM50, atau tunggu 3 pusingan).

Transaksi Pintar: Sistem Auto-Target yang mengenalpasti pemain aktif untuk mempercepatkan proses bayaran dan terima duit.

Sistem Time-Travel (Undo/Redo): Boleh membatalkan atau mengulang semula transaksi jika berlaku kesilapan teknikal atau manusia.

Kad Nasib & Peluang Digital: Koleksi penuh kad Monopoly yang diaplikasikan secara automatik kepada baki akaun pemain.

Tablet Optimized: UI yang menyokong mod skrin penuh (Fullscreen) dengan sistem auto-scaling dan single-tap responsiveness.

🛠️ Tech Stack & Keperluan
Bahasa: C++

Grafik Library: Raylib

NFC API: Windows Smart Card API (WinSCard.lib)

Hardware: Sony RC-S632/U Pasori NFC Reader (atau mana-mana pembaca yang serasi dengan Windows Smart Card).

Platform: Windows (Sesuai untuk Tablet x86/x64).

📂 Struktur Folder
Plaintext
MonopolyBank/
├── include/           # Fail header (.h) Raylib
├── lib/               # Fail library (.lib) Raylib 32-bit/64-bit
├── main.cpp           # Logik utama aplikasi
└── README.md          # Dokumentasi projek
⚙️ Cara Kompilasi (MSVC)
Gunakan Developer Command Prompt for VS dan jalankan arahan berikut:

DOS
cl main.cpp /EHsc /MD /I include /link /LIBPATH:lib raylib.lib winmm.lib gdi32.lib user32.lib shell32.lib winscard.lib
🎮 Cara Penggunaan
Daftar Pemain: Klik "Daftar Pemain" dan tap kad NFC anda atau pilih "Daftar Manual".

Transaksi: Pilih "Terima" atau "Bayar". Sistem akan mensasarkan pemain yang sedang memegang giliran secara automatik. Tekan SAHKAN untuk tamat.

Penjara: Jika pemain masuk penjara, sistem akan mengunci tindakan mereka sehingga mereka bebas melalui balingan dadu atau bayaran denda.

Tamat Giliran: Tekan butang TAMAT GILIRAN >> untuk beralih ke pemain seterusnya.
