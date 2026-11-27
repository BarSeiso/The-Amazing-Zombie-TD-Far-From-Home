(function () {
  var SCENES = [
    ["BUMI / 2042", "Wabah global memutus komunikasi antarkota.", "Saluran darurat mulai menyiarkan pesan terakhir."],
    ["KOTA-KOTA TERISOLASI", "Wabah menyebar dari satu kota ke kota berikutnya.", "Stasiun evakuasi kewalahan menampung warga."],
    ["SETELAH GELOMBANG PERTAMA", "Distrik yang dulu padat kini nyaris kosong.", "Semakin sedikit penyintas yang menjawab panggilan radio."],
    ["JALUR MENUJU SHELTER", "Bu Sari menyelipkan daftar warga di bawah radio.", "Operator: Nanti panggil satu-satu, Bu. Sesudah gerbang ditutup."],
    ["PENUNJUKAN KOMANDAN", "Pos komando menyerahkan kendali pertahanan kepadamu.", "Tempatkan peleton dan jaga gerbang shelter."]
  ];

  var ENDINGS = {
    1: [
      ["PAGAR TIMUR", "Tembakan terakhir berhenti. Pagar masih terkunci.", "Operator: Bu Sari, daftarnya masih di sana?"],
      ["DAFTAR HADIR", "Bu Sari menarik kertas dari bawah radio.", "Bu Sari: Pak Darma?"],
      ["GILIRAN PAGI", "Dari balik pagar seseorang menjawab, \"Hadir, Bu.\"", "Operator menaruh radio dan mengangkat palang."]
    ],
    2: [
      ["GERENDEL", "Gerendel jatuh di sebelah dalam pagar.", "Operator: Tutup pintu ruang pendaftaran!"],
      ["SALURAN TUJUH", "Radio tergeletak di bawah meja. Lampunya menyala.", "Operator: Bu Sari? Jawab kalau dengar."],
      ["PAGI DI SEKTOR 07", "Di balik pagar, semua warga sudah terinfeksi.", "Bu Sari masih menggenggam papan daftar hadir."]
    ]
  };

  var ENEMY_NAMES = ["Pejalan", "Pelari", "Mutan", "Boss", "Berzirah", "Pendobrak"];
  var UNITS = [
    { kind: 0, name: "Archer", price: 55, role: "Cepat & murah" },
    { kind: 1, name: "Cannon", price: 100, role: "Ledakan area" },
    { kind: 2, name: "Sniper", price: 125, role: "Tembus zirah" }
  ];

  var ENEMY_GUIDE = [
    { title: "Pejalan (Walker)", stats: "HP 31 (+5/w) • Bocor -2", note: "Zombie standar bergerombol.", tone: "cream" },
    { title: "Pelari (Runner)", stats: "HP 23 (+4/w) • Bocor -2", note: "Gerak cepat, cegat dengan Archer.", tone: "cream" },
    { title: "Mutan Berat", stats: "HP 110 (+14/w) • Bocor -3", note: "Lambat dengan HP sangat tebal.", tone: "cream" },
    { title: "Berzirah (Wave 6+)", stats: "HP 110 • Tahan 40% dmg", note: "Hanya Sniper memberi 100% damage!", tone: "amber" },
    { title: "Pendobrak (Wave 9+)", stats: "HP 165 • Bocor -5 HP", note: "Berlari cepat di belokan terakhir!", tone: "alert" },
    { title: "Boss Krisis (W11-15)", stats: "HP 550+ (+25%) • Bocor -5", note: "Muncul saat sirene Mode Krisis.", tone: "alert" }
  ];

  function callC(name, argTypes, args) {
    try {
      if (window.Module && typeof window.Module.ccall === "function") {
        window.Module.ccall(name, null, argTypes || [], args || []);
      }
    } catch (e) {
      console.error("Mobile UI cmd error:", name, e);
    }
  }

  function esc(str) {
    return String(str)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  var root = document.getElementById("mobile-ui");
  if (!root) return;

  root.innerHTML =
    '<div id="m-topbar" class="m-topbar hidden">' +
      '<div class="m-stat"><span class="m-k">WAVE</span><span id="m-wave" class="m-v">01/15</span></div>' +
      '<div class="m-stat"><span class="m-k">DANA</span><span id="m-money" class="m-v amber">115</span></div>' +
      '<div class="m-stat m-hp-wrap"><span class="m-k">SHELTER</span><span id="m-hp" class="m-v mint">20/20</span><div class="m-hp-bar"><div id="m-hp-fill"></div></div></div>' +
      '<div class="m-top-actions">' +
        '<button type="button" data-cmd="help" class="m-btn m-btn-sm">INTEL</button>' +
        '<button type="button" data-cmd="pause" class="m-btn m-btn-sm">JEDA</button>' +
      '</div>' +
    '</div>' +
    '<div id="m-banner" class="m-banner hidden"></div>' +
    '<div id="m-dock" class="m-dock hidden"></div>' +
    '<div id="m-story" class="m-story hidden"></div>' +
    '<div id="m-modal" class="m-modal hidden"></div>';

  var topbarEl = document.getElementById("m-topbar");
  var bannerEl = document.getElementById("m-banner");
  var dockEl = document.getElementById("m-dock");
  var storyEl = document.getElementById("m-story");
  var modalEl = document.getElementById("m-modal");
  var waveEl = document.getElementById("m-wave");
  var moneyEl = document.getElementById("m-money");
  var hpEl = document.getElementById("m-hp");
  var hpFillEl = document.getElementById("m-hp-fill");

  var lastDockKey = "";
  var lastStoryKey = "";
  var lastModalKey = "";

  root.addEventListener("click", function (ev) {
    var btn = ev.target.closest("[data-cmd]");
    if (!btn || btn.disabled) return;
    ev.preventDefault();
    var cmd = btn.getAttribute("data-cmd");
    var arg = btn.getAttribute("data-arg");
    switch (cmd) {
      case "play": callC("web_cmd_play"); break;
      case "story-next": callC("web_cmd_story_next"); break;
      case "story-skip": callC("web_cmd_story_skip"); break;
      case "shop": callC("web_cmd_select_shop", ["number"], [Number(arg)]); break;
      case "place-confirm": callC("web_cmd_place_confirm"); break;
      case "place-cancel": callC("web_cmd_place_cancel"); break;
      case "target": callC("web_cmd_tower_target"); break;
      case "upgrade": callC("web_cmd_tower_upgrade"); break;
      case "sell": callC("web_cmd_tower_sell"); break;
      case "deselect": callC("web_cmd_tower_deselect"); break;
      case "start-speed": callC("web_cmd_start_or_speed"); break;
      case "pause": callC("web_cmd_toggle_pause"); break;
      case "help": callC("web_cmd_toggle_help"); break;
      case "settings": callC("web_cmd_toggle_settings"); break;
      case "motion": callC("web_cmd_toggle_motion"); break;
      case "music": callC("web_cmd_adjust_music", ["number"], [Number(arg)]); break;
      case "sfx": callC("web_cmd_adjust_sfx", ["number"], [Number(arg)]); break;
      case "retry": callC("web_cmd_retry"); break;
      case "menu": callC("web_cmd_to_menu"); break;
    }
  });

  function renderDock(d) {
    var full = d.towers >= d.maxTowers;
    var cardsHtml = '<div class="m-shop-grid">';
    for (var i = 0; i < UNITS.length; i++) {
      var u = UNITS[i];
      var active = d.placing === u.kind;
      var afford = d.money >= u.price;
      cardsHtml +=
        '<button type="button" data-cmd="shop" data-arg="' + u.kind + '" class="m-card' +
        (active ? ' active' : '') + (full ? ' muted' : '') + '">' +
          '<div class="m-card-name">' + u.name + '</div>' +
          '<div class="m-card-price ' + (full ? 'muted' : afford ? 'amber' : 'alert') + '">' +
            u.price + ' dana</div>' +
        '</button>';
    }
    cardsHtml += '</div>';

    var detailHtml = "";
    if (d.selected) {
      var s = d.selected;
      var maxLv = s.upCost <= 0;
      var canUp = !maxLv && d.money >= s.upCost;
      detailHtml =
        '<div class="m-inspector">' +
          '<div class="m-ins-head">' +
            '<span class="amber">' + esc(s.name) + ' LV ' + s.level + '</span>' +
            '<span class="muted">' + s.kills + ' kill</span>' +
            '<button type="button" data-cmd="deselect" class="m-x">✕</button>' +
          '</div>' +
          '<div class="m-ins-stats">' +
            '<span>Dmg ' + s.dmg + (maxLv ? '' : '→<b class="mint">' + s.nextDmg + '</b>') + '</span>' +
            '<span>Jarak ' + s.range + (maxLv ? '' : '→<b class="mint">' + s.nextRange + '</b>') + '</span>' +
            '<span>Laju ' + s.reload.toFixed(2) + 's</span>' +
          '</div>' +
          '<div class="m-ins-btns">' +
            '<button type="button" data-cmd="target" class="m-btn m-btn-sm">BIDIK: ' + esc(s.target) + '</button>' +
            '<button type="button" data-cmd="upgrade" class="m-btn m-btn-sm' + (canUp ? ' m-btn-mint' : '') + '"' +
              (canUp ? '' : ' disabled') + '>' + (maxLv ? 'MAKS' : 'UPGRADE ' + s.upCost) + '</button>' +
            '<button type="button" data-cmd="sell" class="m-btn m-btn-sm m-btn-danger">JUAL +' + s.sellVal + '</button>' +
          '</div>' +
        '</div>';
    } else if (d.placing >= 0) {
      var unit = UNITS[d.placing];
      detailHtml =
        '<div class="m-inspector">' +
          '<div class="m-ins-head"><span class="amber">PASANG ' + unit.name.toUpperCase() + ' (' + unit.price + ')</span>' +
          '<span class="muted">' + unit.role + '</span></div>' +
          '<div class="m-ins-btns">' +
            (d.hasAim
              ? '<button type="button" data-cmd="place-confirm" class="m-btn m-btn-primary"' +
                  (d.aimValid ? '' : ' disabled') + '>' + (d.aimValid ? 'PASANG DI SINI' : 'POSISI TERHALANG') + '</button>'
              : '') +
            '<button type="button" data-cmd="place-cancel" class="m-btn">BATAL</button>' +
          '</div>' +
        '</div>';
    } else if (d.state === "PREPARE") {
      var tags = [];
      for (var k = 0; k < ENEMY_NAMES.length; k++) {
        if (d.intel[k] > 0) {
          var cls = k === 5 ? "alert" : k === 4 ? "amber" : "";
          tags.push('<span class="m-tag ' + cls + '">' + ENEMY_NAMES[k] + ' ×' + d.intel[k] + '</span>');
        }
      }
      detailHtml =
        '<div class="m-intel">' +
          '<div class="m-intel-head"><span>INTEL WAVE ' + String(d.wave).padStart(2, "0") + '</span>' +
          '<span class="muted">Peleton ' + d.towers + '/' + d.maxTowers + '</span></div>' +
          '<div class="m-tags">' + tags.join("") + '</div>' +
        '</div>';
    } else {
      var pct = d.qLen > 0 ? Math.max(0, Math.min(100, Math.round((1 - d.rem / d.qLen) * 100))) : 100;
      detailHtml =
        '<div class="m-intel">' +
          '<div class="m-intel-head">' +
            '<span class="' + (d.crisisWarn || d.state === "CRISIS_WAVE" ? 'alert' : 'amber') + '">' +
              (d.state === "CRISIS_WAVE" ? 'SERBUAN KRISIS!' : d.crisisWarn ? 'SIRENE KRISIS!' : 'PERTEMPURAN') +
            '</span>' +
            '<span>' + (d.fighting ? ('Sisa ' + d.rem + '/' + d.qLen) : (d.kills + ' tewas')) + '</span>' +
          '</div>' +
          '<div class="m-progress"><div style="width:' + pct + '%;background:' +
            (d.state === "CRISIS_WAVE" ? 'var(--red)' : 'var(--mint)') + '"></div></div>' +
        '</div>';
    }

    var actionLabel = d.fighting ? ("LAJU " + d.speed + "×") : (d.state === "PREPARE" ? "MULAI WAVE" : "SIAGA");
    var actionEnabled = d.fighting || d.state === "PREPARE";
    var actionHtml =
      '<button type="button" data-cmd="start-speed" class="m-btn m-btn-primary m-btn-start"' +
      (actionEnabled ? '' : ' disabled') + '>' + actionLabel + '</button>';

    var key = cardsHtml + detailHtml + actionHtml;
    if (key !== lastDockKey) {
      lastDockKey = key;
      dockEl.innerHTML = cardsHtml + detailHtml + actionHtml;
    }
  }

  function renderStory(d) {
    var cap = d.storyKind === 0
      ? (SCENES[d.storyScene] || SCENES[0])
      : ((ENDINGS[d.storyKind] && ENDINGS[d.storyKind][d.storyScene]) || ["", "", ""]);
    var pct = Math.max(0, Math.min(100, Math.round(d.storyPct * 100)));
    var key = d.storyKind + ":" + d.storyScene;
    if (key !== lastStoryKey) {
      lastStoryKey = key;
      storyEl.innerHTML =
        '<div class="m-story-card">' +
          '<div class="m-story-head">' +
            '<span class="amber">' + esc(cap[0]) + '</span>' +
            '<span class="muted">0' + (d.storyScene + 1) + ' / 0' + d.storyTotal + '</span>' +
          '</div>' +
          '<p class="m-story-line">' + esc(cap[1]) + '</p>' +
          '<p class="m-story-line">' + esc(cap[2]) + '</p>' +
          '<div class="m-story-foot">' +
            '<div class="m-progress m-story-prog"><div id="m-story-fill" style="width:' + pct + '%"></div></div>' +
            '<div class="m-story-btns">' +
              '<button type="button" data-cmd="story-skip" class="m-btn m-btn-sm">LEWATI</button>' +
              '<button type="button" data-cmd="story-next" class="m-btn m-btn-sm m-btn-primary">LANJUT</button>' +
            '</div>' +
          '</div>' +
        '</div>';
    } else {
      var fill = document.getElementById("m-story-fill");
      if (fill) fill.style.width = pct + "%";
    }
  }

  function renderModal(d) {
    var html = "";
    if (d.screen === "menu") {
      html =
        '<div class="m-panel m-menu-panel">' +
          '<div class="m-tag-top">PERTAHANAN SHELTER // 2042</div>' +
          '<h2 class="m-title">THE AMAZING ZOMBIE TD</h2>' +
          '<div class="m-sub">FAR FROM HOME</div>' +
          '<p class="m-note">Operator Shelter: Siapkan peleton dan jaga gerbang selama ' + d.waveCount + ' gelombang.</p>' +
          '<div class="m-menu-btns">' +
            '<button type="button" data-cmd="play" class="m-btn m-btn-primary">MULAI OPERASI</button>' +
            '<div class="m-row2">' +
              '<button type="button" data-cmd="settings" class="m-btn">PENGATURAN</button>' +
              '<button type="button" data-cmd="help" class="m-btn">PANDUAN MUSUH</button>' +
            '</div>' +
          '</div>' +
          '<div class="m-best ' + (d.best > 0 ? 'mint' : 'muted') + '">' +
            (d.best > 0 ? ('Skor Terbaik: ' + d.best + ' poin') : 'Belum ada rekor operasi') +
          '</div>' +
        '</div>';
    } else if (d.screen === "settings") {
      html =
        '<div class="m-panel">' +
          '<div class="m-modal-head"><span class="m-modal-title">PENGATURAN</span>' +
            '<button type="button" data-cmd="settings" class="m-btn m-btn-sm">TUTUP</button></div>' +
          '<div class="m-set-row">' +
            '<div><div class="m-set-lbl">Kurangi Animasi</div><div class="m-set-sub">Redupkan kedipan visual</div></div>' +
            '<button type="button" data-cmd="motion" class="m-btn m-btn-sm' + (d.reduced ? ' m-btn-primary' : '') + '">' +
              (d.reduced ? 'AKTIF' : 'NONAKTIF') + '</button>' +
          '</div>' +
          '<div class="m-set-row">' +
            '<div><div class="m-set-lbl">Volume Musik</div><div class="m-set-sub amber">' + d.music + '%</div></div>' +
            '<div class="m-vol-btns">' +
              '<button type="button" data-cmd="music" data-arg="-10" class="m-btn m-btn-sm"' + (d.music <= 0 ? ' disabled' : '') + '>−</button>' +
              '<button type="button" data-cmd="music" data-arg="10" class="m-btn m-btn-sm"' + (d.music >= 100 ? ' disabled' : '') + '>+</button>' +
            '</div>' +
          '</div>' +
          '<div class="m-set-row">' +
            '<div><div class="m-set-lbl">Volume Efek (SFX)</div><div class="m-set-sub mint">' + d.sfx + '%</div></div>' +
            '<div class="m-vol-btns">' +
              '<button type="button" data-cmd="sfx" data-arg="-10" class="m-btn m-btn-sm"' + (d.sfx <= 0 ? ' disabled' : '') + '>−</button>' +
              '<button type="button" data-cmd="sfx" data-arg="10" class="m-btn m-btn-sm"' + (d.sfx >= 100 ? ' disabled' : '') + '>+</button>' +
            '</div>' +
          '</div>' +
        '</div>';
    } else if (d.screen === "help") {
      var list = "";
      for (var i = 0; i < ENEMY_GUIDE.length; i++) {
        var e = ENEMY_GUIDE[i];
        list +=
          '<div class="m-guide-item">' +
            '<div class="m-guide-title ' + e.tone + '">' + e.title + '</div>' +
            '<div class="m-guide-stat">' + e.stats + '</div>' +
            '<div class="m-guide-note">' + e.note + '</div>' +
          '</div>';
      }
      html =
        '<div class="m-panel m-panel-wide">' +
          '<div class="m-modal-head"><span class="m-modal-title amber">INTELIJEN MUSUH &amp; TAKTIK</span>' +
            '<button type="button" data-cmd="help" class="m-btn m-btn-sm">TUTUP</button></div>' +
          '<div class="m-guide-grid">' + list + '</div>' +
          '<div class="m-guide-tip">Tips: Jual 1 Archer di wave akhir jika slot 6/6 penuh untuk menambah Sniper/Cannon penahan Mode Krisis.</div>' +
        '</div>';
    } else if (d.screen === "pause") {
      html =
        '<div class="m-panel">' +
          '<div class="m-modal-title">OPERASI DIJEDA</div>' +
          '<p class="m-note ' + (d.confirmMenu ? 'alert' : '') + '">' +
            (d.confirmMenu ? 'Progres sesi ini akan hilang. Yakin ke Menu?' : 'Simulasi dan audio dihentikan sementara.') +
          '</p>' +
          '<div class="m-menu-btns">' +
            '<button type="button" data-cmd="pause" class="m-btn m-btn-primary">LANJUTKAN</button>' +
            '<button type="button" data-cmd="settings" class="m-btn">PENGATURAN</button>' +
            '<button type="button" data-cmd="menu" class="m-btn' + (d.confirmMenu ? ' m-btn-danger' : '') + '">' +
              (d.confirmMenu ? 'YAKIN KE MENU?' : 'MENU UTAMA') + '</button>' +
          '</div>' +
        '</div>';
    } else if (d.screen === "result") {
      var won = d.state === "WON";
      html =
        '<div class="m-panel">' +
          '<div class="m-modal-head">' +
            '<span class="m-modal-title ' + (won ? 'mint' : 'alert') + '">' + (won ? 'SHELTER BERTAHAN' : 'SHELTER JATUH') + '</span>' +
            (d.newRec ? '<span class="amber m-rec">REKOR BARU!</span>' : '') +
          '</div>' +
          '<div class="m-report">' +
            '<div><span>Peringkat</span><b class="amber">' + esc(d.rank) + ' (' + d.score + ')</b></div>' +
            '<div><span>Gelombang</span><b>' + (won ? d.waveCount + '/' + d.waveCount : 'Wave ' + d.wave) + '</b></div>' +
            '<div><span>Zombie Tewas</span><b>' + d.kills + ' ekor</b></div>' +
            '<div><span>Sisa HP</span><b class="' + (won ? 'mint' : 'alert') + '">' + d.hp + '/' + d.maxHp + '</b></div>' +
            '<div><span>Mode Krisis</span><b>' + (d.crisisCleared ? 'Ditahan (W' + d.crisisWave + ')' : 'Wave ' + d.crisisWave) + '</b></div>' +
            '<div><span>Total Dana</span><b class="amber">' + d.earned + ' dana</b></div>' +
          '</div>' +
          '<div class="m-row2">' +
            '<button type="button" data-cmd="retry" class="m-btn m-btn-primary">' + (won ? 'OPERASI BARU' : 'COBA LAGI') + '</button>' +
            '<button type="button" data-cmd="menu" class="m-btn">MENU UTAMA</button>' +
          '</div>' +
        '</div>';
    }

    if (html !== lastModalKey) {
      lastModalKey = html;
      modalEl.innerHTML = html;
    }
    modalEl.classList.toggle("hidden", !html);
  }

  window.__ffhUpdateMobileUI = function (jsonText) {
    var d;
    try {
      d = JSON.parse(jsonText);
    } catch (e) {
      return;
    }

    var inGame = d.screen === "play" || d.screen === "pause" || d.screen === "result";
    topbarEl.classList.toggle("hidden", !inGame);
    bannerEl.classList.toggle("hidden", !inGame);
    dockEl.classList.toggle("hidden", !inGame);
    storyEl.classList.toggle("hidden", d.screen !== "story");

    document.body.classList.toggle("m-crisis-warn", !!d.crisisWarn);
    document.body.classList.toggle("m-in-story", d.screen === "story");

    if (inGame) {
      waveEl.textContent = String(d.wave).padStart(2, "0") + "/" + String(d.waveCount).padStart(2, "0");
      moneyEl.textContent = String(d.money);
      hpEl.textContent = String(d.hp).padStart(2, "0") + "/" + d.maxHp;
      hpEl.className = "m-v " + (d.hp <= 5 ? "alert" : "mint");
      hpFillEl.style.width = Math.max(0, Math.min(100, (d.hp / d.maxHp) * 100)) + "%";
      hpFillEl.style.background = d.hp <= 5 ? "var(--red)" : "var(--mint)";

      bannerEl.textContent = d.banner;
      bannerEl.className = "m-banner tone-" + d.tone;
      renderDock(d);
    }

    if (d.screen === "story") {
      renderStory(d);
    }
    renderModal(d);
  };
})();
