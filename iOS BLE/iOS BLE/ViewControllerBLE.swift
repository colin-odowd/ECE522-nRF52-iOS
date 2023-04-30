//
//  ViewControllerBT.swift
//  iOS BLE
//
//  Created by Colin O'Dowd on 03/01/2023
//

// Import necessary modules
import UIKit
import Foundation
import Charts

enum SensorTag: Int {
    case HRM = 1
    case BR = 2
    case OxySat = 3
    case Temp = 4
    case Hum = 5
    case Accel = 6
    case Mic = 7
}

extension Notification.Name {
    static let newChartDataReceived = Notification.Name("newChartDataReceived")
}

class ViewControllerBLE: UIViewController, HomeViewControllerDelegate {

    // Variable Initializations
    var sendChartData: [ChartDataEntry] = []
    var chartEntriesHRM: [ChartDataEntry] = []
    var chartEntriesBR: [ChartDataEntry] = []
    var chartEntriesOxySat: [ChartDataEntry] = []
    var chartEntriesTemp: [ChartDataEntry] = []
    var chartEntriesHum: [ChartDataEntry] = []
    var chartEntriesAccel: [ChartDataEntry] = []
    var chartEntriesMic: [ChartDataEntry] = []
    var sendSensorType: String?
    
    var viewControllerChart: ViewControllerChart?

    @IBOutlet weak var sensorHRMLabel: UILabel!
    @IBOutlet weak var sensorBRLabel: UILabel!
    @IBOutlet weak var sensorOxySatLabel: UILabel!
    @IBOutlet weak var sensorTempLabel: UILabel!
    @IBOutlet weak var sensorHumLabel: UILabel!
    @IBOutlet weak var sensorAccelLabel: UILabel!

    @IBOutlet weak var sensorHRMValue: UILabel!
    @IBOutlet weak var sensorBRValue: UILabel!
    @IBOutlet weak var sensorOxySatValue: UILabel!
    @IBOutlet weak var sensorTempValue: UILabel!
    @IBOutlet weak var sensorHumValue: UILabel!
    @IBOutlet weak var sensorAccelValue: UILabel!

    @IBOutlet weak var lineChartView: LineChartView!
    
    var bluetoothManager: ViewControllerHome?
    
    // This function is called before the storyboard view is loaded onto the screen.
    // Runs only once.
    override func viewDidLoad() {
        super.viewDidLoad()

        sensorHRMLabel.isUserInteractionEnabled = true
        sensorBRLabel.isUserInteractionEnabled = true
        sensorOxySatLabel.isUserInteractionEnabled = true
        sensorTempLabel.isUserInteractionEnabled = true
        sensorHumLabel.isUserInteractionEnabled = true
        sensorAccelLabel.isUserInteractionEnabled = true
        
        sensorHRMValue.isUserInteractionEnabled = true
        sensorBRValue.isUserInteractionEnabled = true
        sensorOxySatValue.isUserInteractionEnabled = true
        sensorTempValue.isUserInteractionEnabled = true
        sensorHumValue.isUserInteractionEnabled = true
        sensorAccelValue.isUserInteractionEnabled = true
        
        // Add tap gesture recognizers to the labels
        let tapGestureHRM = UITapGestureRecognizer(target: self, action: #selector(labelTapped(_:)))
        tapGestureHRM.numberOfTapsRequired = 1
        tapGestureHRM.numberOfTouchesRequired = 1
        
        let tapGestureBR = UITapGestureRecognizer(target: self, action: #selector(labelTapped(_:)))
        let tapGestureOxySat = UITapGestureRecognizer(target: self, action: #selector(labelTapped(_:)))
        let tapGestureTemp = UITapGestureRecognizer(target: self, action: #selector(labelTapped(_:)))
        let tapGestureHum = UITapGestureRecognizer(target: self, action: #selector(labelTapped(_:)))
        let tapGestureAccel = UITapGestureRecognizer(target: self, action: #selector(labelTapped(_:)))

        sensorHRMLabel.addGestureRecognizer(tapGestureHRM)
        sensorHRMValue.addGestureRecognizer(tapGestureHRM)
        
        sensorBRLabel.addGestureRecognizer(tapGestureBR)
        sensorBRValue.addGestureRecognizer(tapGestureBR)

        sensorOxySatLabel.addGestureRecognizer(tapGestureOxySat)
        sensorOxySatValue.addGestureRecognizer(tapGestureOxySat)

        sensorTempLabel.addGestureRecognizer(tapGestureTemp)
        sensorTempValue.addGestureRecognizer(tapGestureTemp)

        sensorHumLabel.addGestureRecognizer(tapGestureHum)
        sensorHumValue.addGestureRecognizer(tapGestureHum)

        sensorAccelLabel.addGestureRecognizer(tapGestureAccel)
        sensorAccelValue.addGestureRecognizer(tapGestureAccel)

        // Set the tag property of each label to a unique value
        sensorHRMLabel.tag = SensorTag.HRM.rawValue
        sensorHRMValue.tag = SensorTag.HRM.rawValue
        sensorBRLabel.tag = SensorTag.BR.rawValue
        sensorBRValue.tag = SensorTag.BR.rawValue
        sensorOxySatLabel.tag = SensorTag.OxySat.rawValue
        sensorOxySatValue.tag = SensorTag.OxySat.rawValue
        sensorTempLabel.tag = SensorTag.Temp.rawValue
        sensorTempValue.tag = SensorTag.Temp.rawValue
        sensorHumLabel.tag = SensorTag.Hum.rawValue
        sensorHumValue.tag = SensorTag.Hum.rawValue
        sensorAccelLabel.tag = SensorTag.Accel.rawValue
        sensorAccelValue.tag = SensorTag.Accel.rawValue
        
        sensorHRMLabel.textColor = UIColor.white
        sensorBRLabel.textColor = UIColor.white
        sensorOxySatLabel.textColor = UIColor.white
        sensorTempLabel.textColor = UIColor.white
        sensorHumLabel.textColor = UIColor.white
        sensorAccelLabel.textColor = UIColor.white
        
        sensorHRMValue.textColor = UIColor.white
        sensorBRValue.textColor = UIColor.white
        sensorOxySatValue.textColor = UIColor.white
        sensorTempValue.textColor = UIColor.white
        sensorHumValue.textColor = UIColor.white
        sensorAccelValue.textColor = UIColor.white
        
        setupBackgroundImage()
    }
    
    func setupBackgroundImage() {
        // Add a background image
        let backgroundImage = UIImage(named: "ble_background.jpg")
        let backgroundImageView = UIImageView(image: backgroundImage)
        backgroundImageView.contentMode = .scaleAspectFill
        backgroundImageView.frame = view.bounds
        view.addSubview(backgroundImageView)
        view.sendSubviewToBack(backgroundImageView)
    }
    
    func receivedData(data: Data) {
        
        var entry = ChartDataEntry(x: 0, y: 0)
        
        if let message = String(data: data, encoding: .utf8) {
            print(message)
            
            let components = message.components(separatedBy: ":")
            let sensorString = components[0].trimmingCharacters(in: .whitespacesAndNewlines)
            let sensorValue = components[1].trimmingCharacters(in: .whitespacesAndNewlines)
            let sensorDouble = Double(sensorValue)
            
            switch sensorString {
                case "HRM":
                    sensorHRMValue.text = (sensorValue) + " bpm"
                    entry = ChartDataEntry(x: Double(chartEntriesHRM.count), y: sensorDouble!)
                    chartEntriesHRM.append(entry)
                case "BR":
                    sensorBRValue.text = (sensorValue) + " bpm"
                    entry = ChartDataEntry(x: Double(chartEntriesBR.count), y: sensorDouble!)
                    chartEntriesBR.append(entry)
                case "OxySat":
                    sensorOxySatValue.text = (sensorValue) + " %"
                    entry = ChartDataEntry(x: Double(chartEntriesOxySat.count), y: sensorDouble!)
                    chartEntriesOxySat.append(entry)
                case "Temp":
                    sensorTempValue.text = (sensorValue) + " °C"
                    entry = ChartDataEntry(x: Double(chartEntriesTemp.count), y: sensorDouble!)
                    chartEntriesTemp.append(entry)
                case "Hum":
                    sensorHumValue.text = (sensorValue) + " %"
                    entry = ChartDataEntry(x: Double(chartEntriesHum.count), y: sensorDouble!)
                    chartEntriesHum.append(entry)
                case "Accel":
                    sensorAccelValue.text = (sensorValue) + " g"
                    entry = ChartDataEntry(x: Double(chartEntriesAccel.count), y: sensorDouble!)
                    chartEntriesAccel.append(entry)
                case "Alarm":
                    print("Default Case")
                default:
                    print("Default Case")
            }
            let userInfo: [AnyHashable: Any] = ["components": components, "entry": entry]
            NotificationCenter.default.post(name: .newChartDataReceived, object: self, userInfo: userInfo)
        }
    }
    
    @objc func labelTapped(_ sender: UITapGestureRecognizer) {
        guard let tag = SensorTag(rawValue: sender.view?.tag ?? 0) else {
            return
        }
        
        switch tag {
        case .HRM:
            sendChartData = chartEntriesHRM
            sendSensorType = "HRM"
            performSegue(withIdentifier: "toChartView", sender: self)
            print("HRM label tapped")
        case .BR:
            sendChartData = chartEntriesBR
            sendSensorType = "BR"
            performSegue(withIdentifier: "toChartView", sender: self)
            print("BR label tapped")
        case .OxySat:
            sendChartData = chartEntriesOxySat
            sendSensorType = "OxySat"
            performSegue(withIdentifier: "toChartView", sender: self)
            print("OxySat label tapped")
        case .Temp:
            sendChartData = chartEntriesTemp
            sendSensorType = "Temp"
            performSegue(withIdentifier: "toChartView", sender: self)
            print("Temp label tapped")
        case .Hum:
            sendChartData = chartEntriesHum
            sendSensorType = "Hum"
            performSegue(withIdentifier: "toChartView", sender: self)
            print("Hum label tapped")
        case .Accel:
            sendChartData = chartEntriesAccel
            sendSensorType = "Accel"
            performSegue(withIdentifier: "toChartView", sender: self)
            print("Accel label tapped")
        default:
            print("Default Case")
        }
    }

    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        if segue.identifier == "toChartView" {
            // Get the destination view controller and set the data property
            let destinationVC = segue.destination as! ViewControllerChart
            destinationVC.receivedData = sendChartData
            destinationVC.receivedSensor = sendSensorType
        }
    }
    
}
