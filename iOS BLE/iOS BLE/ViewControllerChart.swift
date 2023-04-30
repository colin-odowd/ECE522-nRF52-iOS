//
//  ViewControllerChart.swift
//  iOS BLE
//
//  Created by Colin O'Dowd on 4/01/23.
//

// Import necessary modules
import UIKit
import Foundation
import Charts

class ViewControllerChart: UIViewController {
    
    @IBOutlet weak var lineChart: LineChartView!
    @IBOutlet weak var labelSensorName: UILabel!
    @IBOutlet weak var labelSensorValue: UILabel!

    // Add this property in your destination view controller class
    var receivedData: [ChartDataEntry] = []
    var receivedSensor: String?
    var chartData = LineChartData()
    var stringSensorLabel = ""


    override func viewDidLoad() {
        super.viewDidLoad()
        
        let backgroundImage = UIImage(named: "ble_background")
        let backgroundImageView = UIImageView(image: backgroundImage)
        backgroundImageView.contentMode = .scaleAspectFill
        backgroundImageView.frame = view.bounds
        view.addSubview(backgroundImageView)
        view.sendSubviewToBack(backgroundImageView)
        
        NotificationCenter.default.addObserver(self, selector: #selector(updateChart(notification:)), name: .newChartDataReceived, object: nil)
    }
    
    @objc func updateChart(notification: Notification) {

        var dataSet: LineChartDataSet? = nil // Initialize dataSet to nil
        
        if let userInfo = notification.userInfo,
               let components = userInfo["components"] as? [String],
               let newEntry = userInfo["entry"] as? ChartDataEntry {
            
            if receivedSensor == nil {
               receivedSensor = components[0]
            }
            
            
            if (receivedSensor == components[0]) {
                receivedData.append(newEntry)
                stringSensorLabel = String(newEntry.y)
            }
            
            switch receivedSensor {
               case "HRM":
                    labelSensorName.text = "Heart Rate"
                    labelSensorValue.text = stringSensorLabel + " bpm"
                case "BR":
                    labelSensorName.text = "Breathing Rate"
                    labelSensorValue.text = stringSensorLabel + " bpm"
                case "OxySat":
                    labelSensorName.text = "Oxygen Sat"
                    labelSensorValue.text = stringSensorLabel + " %"
                case "Temp":
                    labelSensorName.text = "Temperature"
                    labelSensorValue.text = stringSensorLabel + " °C"
                case "Hum":
                    labelSensorName.text = "Humidity"
                    labelSensorValue.text = stringSensorLabel + " %"
                case "Accel":
                    labelSensorName.text = "Acceleration"
                    labelSensorValue.text = stringSensorLabel + " g"
               default:
                   print("Default Case")
               }

            dataSet = LineChartDataSet(entries: receivedData, label: "")

            dataSet?.lineWidth = 2.0 // Set the line width
            dataSet?.setColor(UIColor.white) // Set the line color
            dataSet?.drawCirclesEnabled = false // Disable drawing circles (dots) for data points
            dataSet?.drawValuesEnabled = false // Disable drawing values above data points
            dataSet?.mode = .cubicBezier // Set the line mode to cubic Bezier for a smoother curve
            lineChart.highlightPerTapEnabled = false
            lineChart.highlightPerDragEnabled = false
            
            // Customize LineChartView
            lineChart.xAxis.drawGridLinesEnabled = false
            lineChart.leftAxis.drawGridLinesEnabled = false
            lineChart.leftAxis.labelFont = UIFont.systemFont(ofSize: 16)
            lineChart.rightAxis.enabled = false
            lineChart.legend.enabled = false
            lineChart.xAxis.enabled = false
            lineChart.leftAxis.drawAxisLineEnabled = false
            lineChart.leftAxis.labelTextColor = .white

            let data = LineChartData(dataSet: dataSet!)
            lineChart.data = data
            lineChart.notifyDataSetChanged()
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self, name: .newChartDataReceived, object: nil)
    }
    
}
