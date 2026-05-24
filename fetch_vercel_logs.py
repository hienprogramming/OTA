#!/usr/bin/env python3
"""
Script to fetch Vercel build logs and save them locally.
Usage: python fetch_vercel_logs.py
"""

import os
import requests
import json
from datetime import datetime
from pathlib import Path

# Configuration
VERCEL_API_TOKEN = os.getenv('VERCEL_TOKEN', '')
VERCEL_PROJECT_ID = os.getenv('VERCEL_PROJECT_ID', '')
VERCEL_TEAM_ID = os.getenv('VERCEL_TEAM_ID', '')  # Optional, for team projects
LOG_DIR = Path(__file__).parent / 'Log'

def get_latest_deployment():
    """Get the latest deployment from Vercel API"""
    if not VERCEL_API_TOKEN:
        print("Error: VERCEL_TOKEN not set")
        return None
    
    if not VERCEL_PROJECT_ID:
        print("Error: VERCEL_PROJECT_ID not set")
        return None
    
    headers = {
        'Authorization': f'Bearer {VERCEL_API_TOKEN}',
        'Content-Type': 'application/json'
    }
    
    # Get deployments list
    params = {'limit': 1}
    if VERCEL_TEAM_ID:
        params['teamId'] = VERCEL_TEAM_ID
    
    url = f'https://api.vercel.com/v6/deployments'
    try:
        response = requests.get(url, headers=headers, params=params)
        response.raise_for_status()
        data = response.json()
        
        if data.get('deployments') and len(data['deployments']) > 0:
            return data['deployments'][0]
        else:
            print("No deployments found")
            return None
    except Exception as e:
        print(f"Error fetching deployments: {e}")
        return None

def get_build_logs(deployment_id):
    """Get build logs for a specific deployment"""
    if not VERCEL_API_TOKEN:
        print("Error: VERCEL_TOKEN not set")
        return None
    
    headers = {
        'Authorization': f'Bearer {VERCEL_API_TOKEN}',
        'Content-Type': 'application/json'
    }
    
    params = {}
    if VERCEL_TEAM_ID:
        params['teamId'] = VERCEL_TEAM_ID
    
    url = f'https://api.vercel.com/v3/deployments/{deployment_id}/builds'
    try:
        response = requests.get(url, headers=headers, params=params)
        response.raise_for_status()
        data = response.json()
        return data
    except Exception as e:
        print(f"Error fetching build logs: {e}")
        return None

def save_logs(deployment_data, build_logs_data):
    """Save deployment and build logs to file"""
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    
    # Create filename with timestamp
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    log_filename = LOG_DIR / f'vercel_deployment_{timestamp}.json'
    
    # Combine deployment and build info
    combined_log = {
        'timestamp': datetime.now().isoformat(),
        'deployment': deployment_data,
        'build_logs': build_logs_data
    }
    
    try:
        with open(log_filename, 'w', encoding='utf-8') as f:
            json.dump(combined_log, f, indent=2, ensure_ascii=False)
        print(f"✓ Logs saved to: {log_filename}")
        return True
    except Exception as e:
        print(f"Error saving logs: {e}")
        return False

def save_logs_text(deployment_data, build_logs_data):
    """Save deployment and build logs as readable text file"""
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    
    # Create filename with timestamp
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    log_filename = LOG_DIR / f'vercel_deployment_{timestamp}.txt'
    
    try:
        with open(log_filename, 'w', encoding='utf-8') as f:
            f.write("=" * 80 + "\n")
            f.write(f"Vercel Deployment Log - {datetime.now().isoformat()}\n")
            f.write("=" * 80 + "\n\n")
            
            # Deployment Info
            if deployment_data:
                f.write("DEPLOYMENT INFO:\n")
                f.write("-" * 80 + "\n")
                f.write(f"Deployment ID: {deployment_data.get('uid')}\n")
                f.write(f"Status: {deployment_data.get('state')}\n")
                f.write(f"URL: {deployment_data.get('url')}\n")
                f.write(f"Created: {deployment_data.get('createdAt')}\n")
                f.write(f"Branch: {deployment_data.get('meta', {}).get('githubCommitRef', 'N/A')}\n")
                f.write(f"Commit: {deployment_data.get('meta', {}).get('githubCommitSha', 'N/A')}\n")
                f.write("\n")
            
            # Build Logs
            if build_logs_data:
                f.write("BUILD LOGS:\n")
                f.write("-" * 80 + "\n")
                if isinstance(build_logs_data, dict):
                    f.write(json.dumps(build_logs_data, indent=2, ensure_ascii=False))
                else:
                    f.write(str(build_logs_data))
                f.write("\n")
        
        print(f"✓ Text logs saved to: {log_filename}")
        return True
    except Exception as e:
        print(f"Error saving text logs: {e}")
        return False

def main():
    """Main function"""
    print("Fetching latest Vercel deployment logs...\n")
    
    # Get latest deployment
    deployment = get_latest_deployment()
    if not deployment:
        print("Failed to fetch deployment info")
        return False
    
    deployment_id = deployment.get('uid')
    print(f"Latest deployment ID: {deployment_id}")
    print(f"Status: {deployment.get('state')}")
    print(f"URL: {deployment.get('url')}\n")
    
    # Get build logs
    print("Fetching build logs...")
    build_logs = get_build_logs(deployment_id)
    
    # Save logs
    print("\nSaving logs...")
    save_logs(deployment, build_logs)
    save_logs_text(deployment, build_logs)
    
    print("\n✓ All logs saved successfully!")
    return True

if __name__ == '__main__':
    main()
