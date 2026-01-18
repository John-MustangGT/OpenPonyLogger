#!/usr/bin/env python3
"""
OpenPony Log to CSV Converter

Converts .opl binary log files to CSV format with optional GPS Kalman filtering.

Features:
- Exports GPS, accelerometer, gyroscope, and compass data
- UTC timestamp conversion (when GPS lock available)
- Kalman filtering for smoothing noisy GPS position and velocity
- Configurable output columns
- Multiple CSV files per data type or single merged CSV

Usage:
    # Export all data types to separate CSVs
    python opl_to_csv.py logfile.opl
    
    # Export only GPS data with Kalman filtering
    python opl_to_csv.py logfile.opl --gps-only --kalman
    
    # Export to single merged CSV
    python opl_to_csv.py logfile.opl --merged output.csv
    
    # Custom Kalman filter parameters
    python opl_to_csv.py logfile.opl --kalman --process-noise 0.1 --measurement-noise 5.0
"""

import argparse
import csv
import sys
from pathlib import Path
from typing import List, Dict, Optional
from datetime import datetime

try:
    from openpony_decoder import OpenPonyDecoder, SessionHeader, Sample
except ImportError:
    print("Error: openpony_decoder.py not found in same directory")
    sys.exit(1)

import numpy as np


class KalmanFilter:
    """
    Simple Kalman filter for GPS position and velocity smoothing
    
    State vector: [lat, lon, lat_velocity, lon_velocity]
    """
    
    def __init__(self, process_noise: float = 0.01, measurement_noise: float = 10.0, dt: float = 0.1):
        """
        Initialize Kalman filter
        
        Args:
            process_noise: Process noise covariance (Q) - how much we trust the model
            measurement_noise: Measurement noise covariance (R) - how much we trust GPS
            dt: Time step in seconds (will be updated dynamically)
        """
        # State: [lat, lon, v_lat, v_lon]
        self.state = np.zeros(4)
        self.initialized = False
        
        # State covariance matrix
        self.P = np.eye(4) * 1000  # High initial uncertainty
        
        # Process noise
        self.Q = np.eye(4) * process_noise
        
        # Measurement noise  
        self.R = np.eye(2) * measurement_noise
        
        # Last update time
        self.last_time = None
        
        self.dt = dt
    
    def predict(self, dt: float):
        """Predict next state using motion model"""
        # State transition matrix (constant velocity model)
        F = np.array([
            [1, 0, dt, 0],
            [0, 1, 0, dt],
            [0, 0, 1, 0],
            [0, 0, 0, 1]
        ])
        
        # Predict state
        self.state = F @ self.state
        
        # Predict covariance
        self.P = F @ self.P @ F.T + self.Q
    
    def update(self, measurement: np.ndarray):
        """Update state with GPS measurement [lat, lon]"""
        # Measurement matrix (we only measure position)
        H = np.array([
            [1, 0, 0, 0],
            [0, 1, 0, 0]
        ])
        
        # Innovation (measurement residual)
        y = measurement - H @ self.state
        
        # Innovation covariance
        S = H @ self.P @ H.T + self.R
        
        # Kalman gain
        K = self.P @ H.T @ np.linalg.inv(S)
        
        # Update state
        self.state = self.state + K @ y
        
        # Update covariance
        self.P = (np.eye(4) - K @ H) @ self.P
    
    def process(self, lat: float, lon: float, timestamp: float) -> tuple:
        """
        Process GPS measurement and return filtered position
        
        Args:
            lat: Latitude in degrees
            lon: Longitude in degrees
            timestamp: Unix timestamp in seconds
            
        Returns:
            Tuple of (filtered_lat, filtered_lon, v_lat, v_lon)
        """
        if not self.initialized:
            # Initialize with first measurement
            self.state = np.array([lat, lon, 0, 0])
            self.last_time = timestamp
            self.initialized = True
            return lat, lon, 0.0, 0.0
        
        # Calculate time delta
        dt = timestamp - self.last_time
        if dt <= 0:
            dt = 0.1  # Fallback
        
        self.last_time = timestamp
        
        # Predict
        self.predict(dt)
        
        # Update with measurement
        measurement = np.array([lat, lon])
        self.update(measurement)
        
        return self.state[0], self.state[1], self.state[2], self.state[3]


class OPLtoCSV:
    """Converter from .opl to CSV format"""
    
    def __init__(self, input_file: str, use_kalman: bool = False, 
                 process_noise: float = 0.01, measurement_noise: float = 10.0):
        """
        Initialize converter
        
        Args:
            input_file: Path to .opl file
            use_kalman: Enable Kalman filtering for GPS
            process_noise: Kalman process noise parameter
            measurement_noise: Kalman measurement noise parameter
        """
        self.input_file = input_file
        self.use_kalman = use_kalman
        
        if use_kalman:
            self.kalman = KalmanFilter(
                process_noise=process_noise,
                measurement_noise=measurement_noise
            )
        else:
            self.kalman = None
        
        self.session: Optional[SessionHeader] = None
    
    def export_separate(self, output_dir: str = None):
        """
        Export to separate CSV files by data type
        
        Args:
            output_dir: Output directory (defaults to same as input file)
        """
        if output_dir is None:
            output_dir = Path(self.input_file).parent
        else:
            output_dir = Path(output_dir)
            output_dir.mkdir(parents=True, exist_ok=True)
        
        base_name = Path(self.input_file).stem
        
        # Open CSV files
        csv_files = {}
        csv_writers = {}
        
        try:
            with OpenPonyDecoder(self.input_file) as decoder:
                self.session = decoder.read_session_header()
                
                print(f"Session UUID: {self.session.startup_uuid}")
                print(f"Processing blocks...")
                
                blocks = decoder.read_blocks()
                
                for block_idx, block in enumerate(blocks):
                    for sample in block.samples:
                        # Create CSV file for this type if needed
                        if sample.type not in csv_files:
                            csv_path = output_dir / f"{base_name}_{sample.type}.csv"
                            csv_files[sample.type] = open(csv_path, 'w', newline='')
                            csv_writers[sample.type] = csv.writer(csv_files[sample.type])
                            
                            # Write header
                            header = self._get_csv_header(sample.type)
                            csv_writers[sample.type].writerow(header)
                        
                        # Write sample
                        row = self._sample_to_row(sample)
                        csv_writers[sample.type].writerow(row)
                
                print(f"Exported {len(blocks)} blocks")
                print(f"Created CSV files:")
                for sample_type, f in csv_files.items():
                    print(f"  {f.name}")
        
        finally:
            # Close all CSV files
            for f in csv_files.values():
                f.close()
    
    def export_merged(self, output_file: str):
        """
        Export all samples to a single merged CSV
        
        Args:
            output_file: Output CSV file path
        """
        with open(output_file, 'w', newline='') as f:
            writer = csv.writer(f)
            
            # Write header
            header = ['timestamp_utc', 'timestamp_us', 'type'] + \
                     ['gps_lat', 'gps_lon', 'gps_alt', 'gps_speed', 'gps_heading', 'gps_hdop'] + \
                     ['accel_x', 'accel_y', 'accel_z'] + \
                     ['gyro_x', 'gyro_y', 'gyro_z'] + \
                     ['compass_x', 'compass_y', 'compass_z']
            
            if self.use_kalman:
                header.extend(['gps_lat_filtered', 'gps_lon_filtered', 'gps_v_lat', 'gps_v_lon'])
            
            writer.writerow(header)
            
            with OpenPonyDecoder(self.input_file) as decoder:
                self.session = decoder.read_session_header()
                
                blocks = decoder.read_blocks()
                
                for block in blocks:
                    for sample in block.samples:
                        row = self._sample_to_merged_row(sample)
                        writer.writerow(row)
        
        print(f"Exported to: {output_file}")
    
    def _get_csv_header(self, sample_type: str) -> List[str]:
        """Get CSV header for specific sample type"""
        base = ['timestamp_utc', 'timestamp_us']
        
        if sample_type == 'gps':
            header = base + ['latitude', 'longitude', 'altitude', 'speed', 'heading', 'hdop']
            if self.use_kalman:
                header.extend(['latitude_filtered', 'longitude_filtered', 'velocity_lat', 'velocity_lon'])
            return header
        
        elif sample_type == 'accel':
            return base + ['x', 'y', 'z', 'unit']
        
        elif sample_type == 'gyro':
            return base + ['x', 'y', 'z', 'unit']
        
        elif sample_type == 'compass':
            return base + ['x', 'y', 'z', 'unit']
        
        else:
            return base + ['data']
    
    def _sample_to_row(self, sample: Sample) -> List:
        """Convert sample to CSV row"""
        utc = sample.to_utc(self.session)
        timestamp_str = utc.isoformat() if utc else ''
        
        base = [timestamp_str, sample.timestamp_us]
        
        if sample.type == 'gps':
            row = base + [
                sample.data['latitude'],
                sample.data['longitude'],
                sample.data['altitude'],
                sample.data['speed'],
                sample.data['heading'],
                sample.data['hdop']
            ]
            
            if self.use_kalman and utc:
                lat_f, lon_f, v_lat, v_lon = self.kalman.process(
                    sample.data['latitude'],
                    sample.data['longitude'],
                    utc.timestamp()
                )
                row.extend([lat_f, lon_f, v_lat, v_lon])
            
            return row
        
        elif sample.type in ('accel', 'gyro', 'compass'):
            return base + [
                sample.data['x'],
                sample.data['y'],
                sample.data['z'],
                sample.data.get('unit', '')
            ]
        
        else:
            return base + [str(sample.data)]
    
    def _sample_to_merged_row(self, sample: Sample) -> List:
        """Convert sample to merged CSV row with empty cells for other types"""
        utc = sample.to_utc(self.session)
        timestamp_str = utc.isoformat() if utc else ''
        
        row = [timestamp_str, sample.timestamp_us, sample.type]
        
        # GPS columns
        if sample.type == 'gps':
            row.extend([
                sample.data['latitude'],
                sample.data['longitude'],
                sample.data['altitude'],
                sample.data['speed'],
                sample.data['heading'],
                sample.data['hdop']
            ])
        else:
            row.extend(['', '', '', '', '', ''])
        
        # Accel columns
        if sample.type == 'accel':
            row.extend([sample.data['x'], sample.data['y'], sample.data['z']])
        else:
            row.extend(['', '', ''])
        
        # Gyro columns
        if sample.type == 'gyro':
            row.extend([sample.data['x'], sample.data['y'], sample.data['z']])
        else:
            row.extend(['', '', ''])
        
        # Compass columns
        if sample.type == 'compass':
            row.extend([sample.data['x'], sample.data['y'], sample.data['z']])
        else:
            row.extend(['', '', ''])
        
        # Kalman filtered GPS
        if self.use_kalman and sample.type == 'gps' and utc:
            lat_f, lon_f, v_lat, v_lon = self.kalman.process(
                sample.data['latitude'],
                sample.data['longitude'],
                utc.timestamp()
            )
            row.extend([lat_f, lon_f, v_lat, v_lon])
        elif self.use_kalman:
            row.extend(['', '', '', ''])
        
        return row


def main():
    parser = argparse.ArgumentParser(
        description='Convert OpenPony .opl binary logs to CSV format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Export all data to separate CSV files
  %(prog)s session_12345.opl
  
  # Export GPS only with Kalman filtering
  %(prog)s session_12345.opl --gps-only --kalman
  
  # Export to single merged CSV
  %(prog)s session_12345.opl --merged output.csv
  
  # Custom Kalman parameters
  %(prog)s session_12345.opl --kalman --process-noise 0.05 --measurement-noise 15.0
        """
    )
    
    parser.add_argument('input', help='Input .opl file')
    parser.add_argument('--merged', metavar='FILE', help='Export to single merged CSV file')
    parser.add_argument('--output-dir', '-o', metavar='DIR', help='Output directory for separate CSV files')
    parser.add_argument('--gps-only', action='store_true', help='Export only GPS data')
    parser.add_argument('--kalman', action='store_true', help='Apply Kalman filtering to GPS data')
    parser.add_argument('--process-noise', type=float, default=0.01, 
                       help='Kalman process noise (default: 0.01)')
    parser.add_argument('--measurement-noise', type=float, default=10.0,
                       help='Kalman measurement noise (default: 10.0)')
    
    args = parser.parse_args()
    
    # Check numpy available if Kalman requested
    if args.kalman:
        try:
            import numpy
        except ImportError:
            print("Error: numpy is required for Kalman filtering")
            print("Install with: pip install numpy")
            sys.exit(1)
    
    # Create converter
    converter = OPLtoCSV(
        args.input,
        use_kalman=args.kalman,
        process_noise=args.process_noise,
        measurement_noise=args.measurement_noise
    )
    
    # Export
    try:
        if args.merged:
            converter.export_merged(args.merged)
        else:
            converter.export_separate(args.output_dir)
    
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
