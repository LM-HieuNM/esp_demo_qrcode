#include "web_content.h"

const char index_html[] PROGMEM = R"=====(
<!DOCTYPE HTML>
<html>
<head>
    <title>QR Code Manager</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <link rel="stylesheet" href="/styles.css">
</head>
<body>
    <div class="card">
        <h1>QR Code Manager</h1>
        
        <!-- Form thêm QR mới -->
        <div class="form-group">
            <h2>Add New QR Code</h2>
            <input type="text" id="name" placeholder="Tên chủ tài khoản">
            <input type="text" id="account" placeholder="Số tài khoản">
            <input type="text" id="bank" placeholder="Tên ngân hàng">
            <textarea id="qr_text" placeholder="Nội dung mã QR"></textarea>
            <button class="button" onclick="addQR()">Thêm mới</button>
        </div>

        <!-- Danh sách QR -->
        <div class="qr-list">
            <h2>Danh sách QR Code</h2>
            <div id="qrList">
                <!-- QR items sẽ được thêm vào đây qua JavaScript -->
            </div>
        </div>
    </div>

    <!-- Thông tin hệ thống -->
    <div class="card system-info">
        <h2>System Info</h2>
        <p>Free DRAM: <span id="freeDram">%FREE_DRAM%</span> bytes</p>
        <p>Free PSRAM: <span id="freePsram">%FREE_PSRAM%</span> bytes</p>
        <p>Active QR: <span id="activeQR">None</span></p>
    </div>
    
    <script src="/script.js"></script>
</body>
</html>
)=====";

const char styles_css[] PROGMEM = R"=====(
body {
    font-family: Arial, sans-serif;
    margin: 0;
    padding: 20px;
    background-color: #f0f0f0;
}
.card {
    background-color: white;
    padding: 20px;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    margin-bottom: 20px;
}
.form-group {
    margin-bottom: 20px;
}
.form-group input, 
.form-group textarea {
    width: 100%;
    padding: 8px;
    margin: 8px 0;
    border: 1px solid #ddd;
    border-radius: 4px;
    box-sizing: border-box;
}
.button {
    background-color: #4CAF50;
    border: none;
    color: white;
    padding: 12px 24px;
    text-align: center;
    display: inline-block;
    font-size: 16px;
    margin: 4px 2px;
    cursor: pointer;
    border-radius: 4px;
    transition: background-color 0.3s;
}
.button:hover {
    background-color: #45a049;
}
.qr-item {
    border: 1px solid #ddd;
    padding: 10px;
    margin: 10px 0;
    border-radius: 4px;
    display: flex;
    justify-content: space-between;
    align-items: center;
}
.qr-item.active {
    border-color: #4CAF50;
    background-color: #f8fff8;
}
.delete-btn {
    background-color: #f44336;
}
.delete-btn:hover {
    background-color: #da190b;
}
.button.active-btn {
    background-color: #45a049;
    cursor: default;
    opacity: 0.8;
}

.button:disabled {
    cursor: default;
}

.qr-item.active {
    border: 2px solid #4CAF50;
    background-color: #f8fff8;
}
)=====";

const char script_js[] PROGMEM = R"=====(
// Lưu trữ danh sách QR
let qrList = [];
let activeQRId = null;

// Thêm QR mới
function addQR() {
    const name = document.getElementById('name').value;
    const account = document.getElementById('account').value;
    const bank = document.getElementById('bank').value;
    const qrText = document.getElementById('qr_text').value;

    if (!name || !account || !bank || !qrText) {
        alert('Vui lòng điền đầy đủ thông tin!');
        return;
    }

    const qr = {
        id: Date.now(),
        name: name,
        account: account,
        bank: bank,
        qrText: qrText
    };

    // Gửi lên server
    saveQR(qr);
}

function saveQR(qr) {
    const xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
            if (xhr.status == 200) {
                refreshQRList();
                clearForm();
            } else {
                alert('Lỗi khi lưu QR!');
            }
        }
    };
    xhr.open("POST", "/save-qr", true);
    xhr.setRequestHeader("Content-Type", "application/json");
    xhr.send(JSON.stringify(qr));
}

function activateQR(id) {
    const xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
            activeQRId = id;
            updateQRList();
            document.getElementById('activeQR').textContent = 
                qrList.find(q => q.id === id)?.name || 'None';
        }
    };
    xhr.open("POST", "/activate-qr", true);
    xhr.send(id.toString());
}

function deleteQR(id) {
    if (confirm('Bạn có chắc muốn xóa QR này?')) {
        const xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    refreshQRList();
                } else {
                    alert(`Lỗi khi xóa mã QR: ${xhr.status} - ${xhr.responseText}`);
                }
            }
        };
        xhr.open("DELETE", `/qr/${id}`, true);
        xhr.send();
    }
}

function refreshQRList() {
    const xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
            const response = JSON.parse(xhr.responseText);
            qrList = response.qrList;
            activeQRId = response.activeQRId; // Lấy activeQRId từ response
            updateQRList();
        }
    };
    xhr.open("GET", "/qr-list", true);
    xhr.send();
}

function updateQRList() {
    const container = document.getElementById('qrList');
    container.innerHTML = qrList.map(qr => {
        const isActive = qr.id === activeQRId;
        return `
            <div class="qr-item ${isActive ? 'active' : ''}">
                <div>
                    <strong>${qr.name}</strong><br>
                    ${qr.bank} - ${qr.account}
                </div>
                <div>
                    <button class="button" onclick="activateQR(${qr.id})">
                        ${isActive ? 'Activated' : 'Active'}
                    </button>
                    ${isActive ? '' : `
                        <button class="button delete-btn" onclick="deleteQR(${qr.id})">
                            Delete
                        </button>
                    `}
                </div>
            </div>
        `;
    }).join('');

    // Cập nhật active QR display
    const activeQR = qrList.find(q => q.id === activeQRId);
    document.getElementById('activeQR').textContent = 
        activeQR ? activeQR.name : 'None';
}

function clearForm() {
    document.getElementById('name').value = '';
    document.getElementById('account').value = '';
    document.getElementById('bank').value = '';
    document.getElementById('qr_text').value = '';
}

// Auto refresh system info
setInterval(() => {
    const xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
            const info = JSON.parse(xhr.responseText);
            document.getElementById("freeDram").innerHTML = info.dram;
            document.getElementById("freePsram").innerHTML = info.psram;
        }
    };
    xhr.open("GET", "/system-info", true);
    xhr.send();
}, 5000);

// Initial load
refreshQRList();
)=====";