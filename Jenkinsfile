pipeline {
    agent any

    environment {
        APP_NAME    = "logram"
        BUCKET      = "logram"
        SRC_DIR     = "landing"
        S3_ENDPOINT = "https://s3.perek.rest"
        AWS_DEFAULT_REGION = "garage"
        // awscli v2 (≥2.23) по умолчанию шлёт x-amz-content-sha256 в новом streaming-формате,
        // который Garage 1.0.1 не поддерживает. Откатываем на legacy поведение.
        AWS_REQUEST_CHECKSUM_CALCULATION = "when_required"
        AWS_RESPONSE_CHECKSUM_VALIDATION = "when_required"
        GIT_COMMIT_SHORT = sh(script: "git rev-parse --short HEAD", returnStdout: true).trim()
    }

    options {
        timeout(time: 5, unit: 'MINUTES')
        disableConcurrentBuilds()
    }

    stages {
        stage('Checkout') {
            steps { checkout scm }
        }

        stage('Deploy') {
            steps {
                withCredentials([usernamePassword(credentialsId: 'garage-s3',
                                                  usernameVariable: 'AWS_ACCESS_KEY_ID',
                                                  passwordVariable: 'AWS_SECRET_ACCESS_KEY')]) {
                    sh '''
                        echo "$GIT_COMMIT_SHORT" > "$SRC_DIR/.version"
                        aws --endpoint-url "$S3_ENDPOINT" s3 sync "$SRC_DIR/" "s3://$BUCKET/" --delete
                    '''
                }
            }
        }

        stage('Verify') {
            steps {
                sh 'curl -sSf "https://${APP_NAME}.perek.rest/" -o /dev/null'
            }
        }
    }

    post {
        success { echo "Deployed ${APP_NAME}.perek.rest (${GIT_COMMIT_SHORT})" }
        failure { echo "Deploy of ${APP_NAME}.perek.rest failed" }
        always  { cleanWs() }
    }
}