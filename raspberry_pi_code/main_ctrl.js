const Blynk = require('blynk-library');
const { spawn } = require('child_process');

//Define labels
const STATUS_LABELS = [
	"Light Failure", 			//Case 0
	"Abnormal Vibration", 			//Case 1
	"Impact Shock",				//Case 2
	"Light Instability", 			//Case 3
	"Normal",				//Case 4
	"Overheating",				//Case 5
	"Tampering"
];

const EVENT_CODE = "alert_system";

//Blynk Setup
let blynk = null;
let vPinDisplay = null;
let vPinColor = null;
let vPinControl = null;
let currenttoken = "";
let alertsent = false;
let system_on = true;
let lastalerttime = 0;
//Start LoRa Recevier
const lora = spawn('sudo', ['./debug']);
const ALERT_COOLDOWN = 5000;
console.log("Waiting for LoRa Packet...");

//LiSten for output
lora.stdout.on('data', async(chunk) => {
	const lines = chunk.toString().split('\n');
	for(let line of lines){
		line = line.trim();
		if(line.length == 0) continue;
		console.log(`[RAW Data]:${line}`);

		//Extract Token
		if(line.length == 32 && !line.includes('.')){
			if(line !== currenttoken || blynk == null){
				console.log(`Token Detected: ${line}`);
				currenttoken = line; 

				//Connection Handler
				try{
					if(blynk){
						console.log("Restarting Blynk Connection");
						blynk.disconnect();
					}

					blynk = new Blynk.Blynk(currenttoken, options = {
						connector: new Blynk.TcpClient(options = {addr: "blynk.cloud", port: 80})
					});
					vPinStatus = new blynk.VirtualPin(8);
					vPinControl = new blynk.VirtualPin(12);
					blynk.on('connect', () => console.log("Connected!"));
					vPinControl.on('write', (param) => {
						const value =parseInt(param[0]);
						if(value == 1){
							system_on = true;
							console.log("Monitoring Active");
						}else{
							system_on = false;
							console.log("Not Monitoring");

						}
					});
					} 
				catch (e) {console.error("Init Error:",e);}
			}
			else {
			}
		}
		//extract result
		else if(!line.includes('.') && !isNaN(line)) {
			if(system_on == false){
				return;
			}
			const result = parseInt(line);

			if(result >= 0 && result < STATUS_LABELS.length && blynk){
				const statusText = STATUS_LABELS[result];
				console.log(`Result: "${statusText}"`);
				try{
					if(vPinStatus){
						vPinStatus.write(statusText);
					}
				}catch(e) {console.error("Error Write Failed:", e);}

				//Logic Handler
				if(result == 4){
					blynk.setProperty(14, "color", "#23C48E");
					blynk.virtualWrite(14, 255);
					if(alertsent){
						alertsent = false;
					}
				}
				else {
					blynk.setProperty(14, "color", "#D3435C");
					blynk.virtualWrite(14, 255);

					const now = Date.now();
					if(!alertsent && (now - lastalerttime > ALERT_COOLDOWN)){
						console.log(`Alert Triggered: ${statusText}`);
						if(statusText && statusText !== "Normal"){
							await sendBlynkNotification(currenttoken, statusText);
							lastalerttime = now;
							alertsent= true;
						}else if (!alertsent){
						console.log("Alert Skipped");
					}
					}
				}
			}
		}
		else if (line.includes('.') && !isNaN(line)){
			const voltage = parseFloat(line);
			let percentage = ((voltage-2.5) / (3.90-2.5)) * 100;
			if (percentage > 100) percentage = 100;
			if (percentage < 0) percentage = 0;
			percentage = Math.round(percentage);
			console.log("Battery Voltage: ", percentage);
			if(blynk){blynk.virtualWrite(11, percentage);}
		}
	}
});

//HELPER: Send Notification
async function sendBlynkNotification(token, eventname){
	const message = `Alert!! ${eventname}`;
	const description = encodeURIComponent(message);
	const requestURL = `https://blynk.cloud/external/api/logEvent?token=${token}&code=${EVENT_CODE}&description=${description}`;
	await fetch(requestURL);
	console.log("DEBUG URL:", requestURL);

	try{
		const response = await fetch(requestURL);
		const errorText = await response.text();
		console.log("DEBUG Server Response:", errorText);
		if(response.ok) console.log("Alert Notification Sent");
		else console.error(`Alert Failed to Send: ${response.statusText}`);
	}
	catch (error){
		console.error("Network Error:", error);
	}
};

//Error Handling
lora.stderr.on('data', (data) => {
	console.error(`Error: ${data}`);
});
lora.on('close', (code) => {
	console.log(`LoRa Receiver Stopped: ${code}`);
	
});
